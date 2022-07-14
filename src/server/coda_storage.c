#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "../../includes/util.h"
#include "../../includes/coda_storage.h"
#include "../../includes/def_tipodato.h"
#include "../../includes/fun_descrit.h"
#include "../../includes/coda.h"
#include "../../includes/config.h"

extern struct config_struct config;
extern server_status status;
FILE *fl3;
pthread_mutex_t mlog3;




// --------------------------------------- funzioni di utilità ---------------------------------------- //


static inline NodoStorage_t  *allocStorageNode(){

  return malloc(sizeof(NodoStorage_t));

}

static inline CodaStorage_t *allocQueue(){

  return malloc(sizeof(CodaStorage_t));

}

static inline void freeStorageNode(NodoStorage_t *node){

  canc_coda(node->opener_q); free((void*)node);

}

static inline void LockQueue(CodaStorage_t *q){

  LOCK(&q->qlock);

}

static inline void UnlockQueue(CodaStorage_t *q){

  UNLOCK(&q->qlock);

}

static inline void UnlockQueueAndWait(CodaStorage_t *q, NodoStorage_t *node){

  WAIT(&node->filecond, &q->qlock);

}

static inline void UnlockQueueAndSignal(CodaStorage_t *q, NodoStorage_t *node){

  SIGNAL(&node->filecond); UNLOCK(&q->qlock);

}

// ---------------------------------------------- //
//  Restituisce il nodo identificato da pathname  //
//  (da chiamare con lock)                        //
// ---------------------------------------------- //

NodoStorage_t* queue_s_find(CodaStorage_t *q, char *pathname);

// ---------------------------------------------------------------------------------------------------------- //
//  Estrae dalla coda q un numero di files adeguato per permettere l'inserimento di un file di buf_len bytes, //
//  non superando la capacità dello storage in bytes                                                          //
//  (da chiamare con lock (su q))                                                                             //
// ---------------------------------------------------------------------------------------------------------- //

int queue_s_popUntil(CodaStorage_t *q, int buf_len, int *num_poppedFiles, NodoStorage_t **poppedHead);

// ------------------------------------ //
//  Estrae dalla coda q un nodo (file)  //
//  (da chiamare con lock)              //
// ------------------------------------ //

NodoStorage_t *queue_s_pop(CodaStorage_t *q);

// ----------------------------------------------------------------------------------- //
// Comunica al client sul socket fd, i files della coda poppedH, e libera memoria      //
// ----------------------------------------------------------------------------------- //

int send_to_client_and_free(int fd, int num_files, NodoStorage_t *poppedH);





/* ------------------- interfaccia della coda ------------------ */

CodaStorage_t *init_coda_stor(int limit_num_files, unsigned long storage_capacity, FILE *l, pthread_mutex_t ml) {

    fl3 = l;
    mlog3 = ml;
    CodaStorage_t *q = allocQueue();

    if (!q) return NULL;

    q->head = NULL;
    q->tail = NULL;
    q->cur_numfiles = 0;
    q->cur_usedstorage = 0;
    q->max_num_files = 0;
    q->max_used_storage = 0;
    q->replace_occur = 0;
    q->limit_num_files = limit_num_files;
    q->storage_capacity = storage_capacity;


    if (pthread_mutex_init(&q->qlock, NULL) != 0) {

      perror("\e[0;36mSERVER : \e[0;31mERRORE mutex init\e[0m");
      LOCK(&mlog3);
      fprintf(fl3, "SERVER : ERRORE mutex init");
      UNLOCK(&mlog3);

      return NULL;
    }

    return q;
}

// da chiamare quando server "muore" (prima della delete)

void broadcast_coda_stor(CodaStorage_t *q) {

    if (q == NULL) return;

    NodoStorage_t *temp = q->head;
    while(temp != NULL) {
        if (temp->locked) {

            BCAST(&temp->filecond);
        }
        temp = temp->next;
    }

}

// da chiamare quando server "muore"

void canc_coda_stor(CodaStorage_t *q) {

    if (q == NULL) return;

    while(q->head != NULL) {

        NodoStorage_t *p = (NodoStorage_t*)q->head;
        q->head = q->head->next;

        if (p->len > 0) free(p->data);
        if (&p->filecond)  pthread_cond_destroy(&p->filecond);
        freeStorageNode(p);
    }

    if (&q->qlock)  pthread_mutex_destroy(&q->qlock);


    free(q);
}

void queue_s_deleteNodes (NodoStorage_t *head) {

    while(head != NULL) {

        NodoStorage_t *p = (NodoStorage_t*) head;
        head = head->next;

        if (p->len > 0) free(p->data);
        if (&p->filecond)  pthread_cond_destroy(&p->filecond);
        freeStorageNode(head);
    }

}

int ins_coda_stor(CodaStorage_t *q, char *pathname, bool locked, int fd, FILE *l, pthread_mutex_t ml) {

    NodoStorage_t *poppedNode = NULL;

    if ((q == NULL) || (pathname == NULL)) {

        return EINVAL;
    }

    if (config.v > 2){
      printf("\e[0;36mSERVER : ins_coda_stor, fd: %d, pathname: %s\n\e[0m", fd, pathname);
      LOCK(&mlog3);
      fprintf(fl3, "SERVER : ins_coda_stor, fd: %d, pathname: %s\n", fd, pathname);
      UNLOCK(&mlog3);
    }
    fflush(stdout);

    NodoStorage_t *n = allocStorageNode();

    if (!n) return ENOMEM;

    memset(n, '\0', sizeof(NodoStorage_t));

    if (pthread_cond_init(&n->filecond, NULL) != 0) {

      perror("\e[0;36mSERVER : \e[0;31mERRORE mutex cond\e[0m");
      LOCK(&mlog3);
      fprintf(fl3, "SERVER : ERRORE mutex cond");
      UNLOCK(&mlog3);
      return EAGAIN;
    }

    strncpy(n->pathname, pathname, PATH_MAX);
    n->locked = locked;
    n->fd_locker = (locked) ? fd : -1;
    n->locker_can_write = locked;

    n->opener_q = init_coda(l, ml);
    if (!n->opener_q){

        fprintf(stderr, "\e[0;36mSERVER fd : %d, \e[0;31minitQueue fallita\n\e[0m", fd);
        LOCK(&mlog3);
        fprintf(fl3, "SERVER fd : %d, initQueue fallita\n", fd);
        UNLOCK(&mlog3);
        if (&n->filecond)  pthread_cond_destroy(&n->filecond);
        freeStorageNode(n);

        return EAGAIN;
    }
    if (ins_coda(n->opener_q, fd) != 0) {

        fprintf(stderr, "\e[0;36mSERVER fd : %d, \e[0;31mpush opener_q fallita\n\e[0m", fd);
        LOCK(&mlog3);
        fprintf(fl3, "SERVER fd : %d, push opener_q fallita\n", fd);
        UNLOCK(&mlog3);
        if (&n->filecond)  pthread_cond_destroy(&n->filecond);
        freeStorageNode(n);

        return EAGAIN;
    }

    n->next = NULL;


    LockQueue(q);
    if (queue_s_find(q, pathname) != NULL) {

        if (&n->filecond)  pthread_cond_destroy(&n->filecond);
        freeStorageNode(n);

        UnlockQueue(q);

        return EPERM;
    }

    if (q->cur_numfiles == 0) {

        q->head = n;
        q->tail = n;
    } else {

        q->tail->next = n;
        q->tail = n;
    }

    if (q->cur_numfiles == q->limit_num_files) {

        if (config.v > 2){
          printf("\e[0;36mSERVER : Limite di files raggiunto\n\e[0m");
          LOCK(&mlog3);
          fprintf(fl3, "SERVER : Limite di files raggiunto\n");
          UNLOCK(&mlog3);
        }
        if((poppedNode = queue_s_pop(q)) == NULL) {

            UnlockQueue(q);

            return errno;
        }

        q->replace_occur++;
        q->cur_numfiles += 1;
        UnlockQueue(q);

        if (send_to_client_and_free(fd, 1, poppedNode) == -1) {

            return -1;
        }


        return 0;

    } else {

        if (q->max_num_files == q->cur_numfiles) q->max_num_files++;
    }
    q->cur_numfiles += 1;

    UnlockQueue(q);

    msg_risposta_t res;
    memset(&res, '\0', sizeof(msg_risposta_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {

        close(fd);
        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten res queue_s_openFile errore\n\e[0m");
        LOCK(&mlog3);
        fprintf(fl3, "SERVER : writen res queue_s_openFile errore\n");
        UNLOCK(&mlog3);
        fflush(stdout);

        return -1;
    }

    return 0;
}

int updateOpeners_coda_stor(CodaStorage_t *q, char *pathname, bool locked, int fd, FILE *l, pthread_mutex_t ml) {

    if ((q == NULL) || (pathname == NULL)) {

        return EINVAL;
    }

    NodoStorage_t *item;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {

        UnlockQueue(q);

        return ENOENT;
    }

    while (item->locked && item->fd_locker != fd && status != CLOSED) {

        if (config.v > 2){
          printf("\e[0;36mSERVER : queue_s_openFile, fd: %d, file: %s locked, aspetto...\n\e[0m", fd, pathname);
          LOCK(&mlog3);
          fprintf(fl3, "SERVER : queue_s_openFile, fd: %d, file: %s locked, aspetto...\n", fd, pathname);
          UNLOCK(&mlog3);
        }
        UnlockQueueAndWait(q, item);
    }
    if (status == CLOSED) {

        return ENETDOWN;
    }
    if (item->locked) {                                   //&& item->fd_locker == fd
        if (!locked) {                                    //non lo voglio lockare, unlocko

            item->locked = false;
            item->fd_locker = -1;
        }

        item->locker_can_write = false;
    } else {
        if (locked) {

            item->locked = true;
            item->fd_locker = fd;
            item->locker_can_write = false;                //superfluo
        }
    }

    if(item->opener_q == NULL) {

        item->opener_q = init_coda(l, ml);
        if (!q){

            fprintf(stderr, "\e[0;36mSERVER fd : %d, \e[0;31minitQueue fallita\n\e[0m", fd);
            LOCK(&mlog3);
            fprintf(fl3, "SERVER fd : %d, initQueue fallita\n", fd);
            UNLOCK(&mlog3);
            UnlockQueue(q);

            return EAGAIN;
        }
        if (ins_coda(item->opener_q, fd) != 0) {

            fprintf(stderr, "\e[0;36mSERVER fd : %d, \e[0;31mpush opener_q fallita\n\e[0m", fd);
            LOCK(&mlog3);
            fprintf(fl3, "SERVER fd : %d, push opener_q fallita\n", fd);
            UNLOCK(&mlog3);
            UnlockQueue(q);

            return EAGAIN;
        }
    } else if (trova_coda(item->opener_q, fd) == NULL) {

        if(ins_coda(item->opener_q, fd) != 0) {

            fprintf(stderr, "\e[0;36mSERVER fd : %d, \e[0;31mpush opener_q fallita\n\e[0m", fd);
            LOCK(&mlog3);
            fprintf(fl3, "SERVER fd : %d, push opener_q fallita\n", fd);
            UNLOCK(&mlog3);
            UnlockQueue(q);

            return EAGAIN;
        }
    }

    UnlockQueue(q);

    msg_risposta_t res;
    memset(&res, '\0', sizeof(msg_risposta_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd);
        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten res queue_s_openFile errore\n\e[0m");
        LOCK(&mlog3);
        fprintf(fl3, "SERVER : writen res queue_s_openFile errore\n");
        UNLOCK(&mlog3);

        return -1;
    }

    return 0;
}


int readFile_coda_stor(CodaStorage_t *q, char *pathname, int fd) {

    if ((q == NULL) || (pathname == NULL)) {

        return EINVAL;
    }

    NodoStorage_t *item;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {

        UnlockQueue(q);

        return ENOENT;
    }

    if (item->locked && item->fd_locker != fd) {

        UnlockQueue(q);

        return EPERM;
    }

    if (item->locked) {                                                          //&& item->fd_locker == fd

        item->locker_can_write = false;
    }

    if (trova_coda(item->opener_q, fd) == NULL) {

        UnlockQueue(q);

        return EPERM;
    }

    msg_risposta_t res;                                                         // messaggio risposta al client
    memset(&res, '\0', sizeof(msg_risposta_t));
    res.result = 0;
    res.datalen = item->len;
    strcpy(res.pathname, item->pathname);                                       //superfluo

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {                         //invio messaggio al client

        close(fd);

        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten res readFile_coda_stor errore\n\e[0m");
        LOCK(&mlog3);
        fprintf(fl3, "SERVER : writen res readFile_coda_stor errore\n");
        UNLOCK(&mlog3);

        UnlockQueue(q);

        return -1;
    }

    if (writen(fd, item->data, res.datalen) != res.datalen) {

        close(fd);
        if (config.v > 1){
          printf("\e[0;36mSERVER : \e[0;31mwriten data readFile_coda_stor errore\n\e[0m");
          LOCK(&mlog3);
          fprintf(fl3, "SERVER : writen data readFile_coda_stor errore\n");
          UNLOCK(&mlog3);
        }
        fflush(stdout);

        UnlockQueue(q);

        return -1;
    }

    if (config.v > 1){
      printf("\e[0;36mSERVER : Letto (ed inviato) file %s, di lunghezza %d\n\e[0m", item->pathname, item->len);
      LOCK(&mlog3);
      fprintf(fl3, "SERVER : Letto (ed inviato) file %s, di lunghezza %d\n", item->pathname, item->len);
      UNLOCK(&mlog3);
    }
    fflush(stdout);

    UnlockQueue(q);

    return 0;
}

int readNFiles_coda_stor(CodaStorage_t *q, char *pathname, int fd, int n) {

    msg_risposta_t res;                             // messaggio risposta al client
    memset(&res, '\0', sizeof(msg_risposta_t));
    res.result = 0;

    if ((q == NULL) || (pathname == NULL)) {

        return EINVAL;
    }

    LockQueue(q);

    if (q->cur_numfiles < n || n <= 0) res.datalen = q->cur_numfiles;
    else res.datalen = n;

    if (config.v > 1){
      printf("\e[0;36mSERVER : Numero di files che verranno inviati: %d\n\e[0m", res.datalen);
      LOCK(&mlog3);
      fprintf(fl3, "SERVER : Numero di files che verranno inviati: %d\n", res.datalen);
      UNLOCK(&mlog3);
    }

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {           // invio messaggio al client

        close(fd);
        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten res readFile_coda_stor errore\n\e[0m");
        LOCK(&mlog3);
        fprintf(fl3, "SERVER : writen res readFile_coda_stor errore\n");
        UNLOCK(&mlog3);
        fflush(stderr);

        UnlockQueue(q);

        return -1;
    }

    NodoStorage_t *temp = q->head;
    int num = res.datalen;
    int i = 1;
    while (i <= num) {

        res.datalen = temp->len;
        strcpy(res.pathname, temp->pathname);

        if (writen(fd, &res, sizeof(res)) != sizeof(res)) {                   //invio messaggio al client
            close(fd);
            fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten res readFile_coda_stor errore\n\e[0m");
            LOCK(&mlog3);
            fprintf(fl3, "SERVER : writen res readFile_coda_stor errore\n");
            UNLOCK(&mlog3);
            fflush(stderr);

            UnlockQueue(q);

            return -1;
        }

        if (writen(fd, temp->data, res.datalen) != res.datalen) {

            close(fd);
            fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten data readFile_coda_stor errore\n\e[0m");
            LOCK(&mlog3);
            fprintf(fl3, "SERVER : writen data readFile_coda_stor errore\n");
            UNLOCK(&mlog3);
            fflush(stderr);

            UnlockQueue(q);

            return -1;
        }


        if (config.v > 1){
          printf("\e[0;36mSERVER : Letto (ed inviato) file %s, di lunghezza %d\n\e[0m", temp->pathname, temp->len);
          LOCK(&mlog3);
          fprintf(fl3, "SERVER : Letto (ed inviato) file %s, di lunghezza %d\n", temp->pathname, temp->len);
          UNLOCK(&mlog3);
        }
        fflush(stdout);

        temp = temp->next;
        i++;

    }

    UnlockQueue(q);

    return 0;
}


int writeFile_coda_stor(CodaStorage_t *q, char *pathname, int fd, void *buf, int buf_len) {

    if ((q == NULL) || (pathname == NULL) || buf == NULL) {

        return EINVAL;
    }

    NodoStorage_t *item;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {

        UnlockQueue(q);

        return ENOENT;
    }

    if (!(item->locked) || item->fd_locker != fd || !(item->locker_can_write)) {

        UnlockQueue(q);

        return EPERM;
    }

    if (trova_coda(item->opener_q, fd) == NULL) {

        UnlockQueue(q);

        return EPERM;
    }

    if (buf_len > q->storage_capacity) {

        UnlockQueue(q);

        return ENOMEM;
    }

    NodoStorage_t *poppedHead = NULL;
    int num_poppedFiles = 0;
    int result = queue_s_popUntil(q, buf_len, &num_poppedFiles, &poppedHead);
    if (result != 0) {

        UnlockQueue(q);

        return result;
    }

    void *tmp = malloc(buf_len);
    if (tmp == NULL) {

        UnlockQueue(q);

        fprintf(stderr, "\e[0;36mSERVER fd : %d, \e[0;31mqueue_s_writeToFile malloc fallita\n\e[0m", fd);
        LOCK(&mlog3);
        fprintf(fl3, "SERVER fd : %d, queue_s_writeToFile malloc fallita\n", fd);
        UNLOCK(&mlog3);
        return ENOMEM;
    }

    memcpy(tmp, buf, buf_len);

    item->len = buf_len;
    item->data = tmp;

    if (config.v > 1){
      printf("\e[0;36mSERVER : Scritto il file %s, per una lunghezza di %d\n\e[0m", item->pathname, item->len);
      LOCK(&mlog3);
      fprintf(fl3, "SERVER : Scritto il file %s, per una lunghezza di %d\n", item->pathname, item->len);
      UNLOCK(&mlog3);
    }
    fflush(stdout);

    q->cur_usedstorage += item->len;

    if (q->max_used_storage < q->cur_usedstorage) q->max_used_storage = q->cur_usedstorage;

    item->locker_can_write = false;

    UnlockQueue(q);

    if (num_poppedFiles != 0) {

        if (send_to_client_and_free(fd, num_poppedFiles, poppedHead) == -1) {

            return -1;
        }

        return 0;
    }

    msg_risposta_t res;
    memset(&res, '\0', sizeof(msg_risposta_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {

        close(fd);
        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten res writeFile_coda_stor errore\n\e[0m");
        LOCK(&mlog3);
        fprintf(fl3, "SERVER : writen res writeFile_coda_stor errore\n");
        UNLOCK(&mlog3);
        fflush(stderr);

        return -1;
    }

    return 0;
}


int appendToFile_coda_stor(CodaStorage_t *q, char *pathname, int fd, void *buf, int buf_len) {

    if ((q == NULL) || (pathname == NULL)) {

        return EINVAL;
    }

    NodoStorage_t *item = NULL;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {

        UnlockQueue(q);

        return ENOENT;
    }

    if (trova_coda(item->opener_q, fd) == NULL) {

        if (config.v > 2){
          printf("\e[0;36mSERVER : \e[0;31mitem->opener_q == NULL ? %d\n\e[0m", (item->opener_q == NULL));
          LOCK(&mlog3);
          fprintf(fl3, "SERVER : item->opener_q == NULL ? %d\n", (item->opener_q == NULL));
          UNLOCK(&mlog3);
        }
        fflush(stdout);
        UnlockQueue(q);

        return EPERM;
    }


    NodoStorage_t *poppedHead = NULL;
    int num_poppedFiles = 0;
    int result = queue_s_popUntil(q, buf_len, &num_poppedFiles, &poppedHead);
    if (result != 0) {

        UnlockQueue(q);

        return result;
    }

    void *tmp = realloc(item->data, (item->len) + buf_len);
    if (tmp == NULL) {

        UnlockQueue(q);

        fprintf(stderr, "\e[0;36mSERVER fd : %d, \e[0;31mappendToFile_coda_stor realloc fallita\n\e[0m", fd);
        LOCK(&mlog3);
        fprintf(fl3, "SERVER fd : %d, appendToFile_coda_stor realloc fallita\n", fd);
        UNLOCK(&mlog3);

        return ENOMEM;
    }

    memcpy(&tmp[item->len], buf, buf_len);

    item->data = tmp;
    item->len += buf_len;

    if (config.v > 1){
      printf("\e[0;36mSERVER : Append sul file %s, lunghezza attuale di %d\n\e[0m", item->pathname, item->len);
      LOCK(&mlog3);
      fprintf(fl3, "SERVER : Append sul file %s, lunghezza attuale di %d\n", item->pathname, item->len);
      UNLOCK(&mlog3);
    }
    fflush(stdout);

    q->cur_usedstorage += buf_len;

    if (q->max_used_storage < q->cur_usedstorage) q->max_used_storage = q->cur_usedstorage;

    UnlockQueue(q);

    if (num_poppedFiles != 0) {

        if (send_to_client_and_free(fd, num_poppedFiles, poppedHead) == -1) {

            return -1;
        }

        return 0;
    }

    msg_risposta_t res;
    memset(&res, '\0', sizeof(msg_risposta_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {

        close(fd);
        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten res appendToFile_coda_stor errore\n\e[0m");
        LOCK(&mlog3);
        fprintf(fl3, "SERVER : writen res appendToFile_coda_stor errore\n");
        UNLOCK(&mlog3);
        fflush(stderr);

        return -1;
    }

    return 0;
}

int lockFile_coda_stor(CodaStorage_t *q, char *pathname, int fd) {

    if ((q == NULL) || (pathname == NULL)) {

        return EINVAL;
    }

    NodoStorage_t *item;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {

        UnlockQueue(q);

        return ENOENT;
    }

    while (item->locked && item->fd_locker != fd && status != CLOSED) {

        if (config.v > 2){
          printf("\e[0;36mSERVER : lockFile_coda_stor, fd: %d, file: %s locked, aspetto...\n\e[0m", fd, pathname);
          LOCK(&mlog3);
          fprintf(fl3, "SERVER : lockFile_coda_stor, fd: %d, file: %s locked, aspetto...\n", fd, pathname);
          UNLOCK(&mlog3);
        }
        UnlockQueueAndWait(q, item);
    }
    if (status == CLOSED) {

        return ENETDOWN;
    }

    if (trova_coda(item->opener_q, fd) == NULL) {

        UnlockQueue(q);

        return EPERM;
    }

    item->locked = true;
    item->fd_locker = fd;
    item->locker_can_write = false;

    UnlockQueue(q);

    msg_risposta_t res;
    memset(&res, '\0', sizeof(msg_risposta_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {

        close(fd);
        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten res lockFile_coda_stor errore\n\e[0m");
        LOCK(&mlog3);
        fprintf(fl3, "SERVER : writen res lockFile_coda_stor errore\n");
        UNLOCK(&mlog3);
        fflush(stderr);
        return -1;
    }

    return 0;
}


int unlockFile_coda_stor(CodaStorage_t *q, char *pathname, int fd) {

    if ((q == NULL) || (pathname == NULL)) {

        return EINVAL;
    }

    NodoStorage_t *item;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {

        UnlockQueue(q);

        return ENOENT;
    }

    if (!(item->locked) || item->fd_locker != fd) {

        UnlockQueue(q);

        return EPERM;
    }

    if (trova_coda(item->opener_q, fd) == NULL) {

        UnlockQueue(q);

        return EPERM;
    }

    item->locked = false;
    item->fd_locker = -1;
    item->locker_can_write = false;

    UnlockQueueAndSignal(q, item);

    msg_risposta_t res;
    memset(&res, '\0', sizeof(msg_risposta_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {

        close(fd);
        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten res writeFile_coda_stor errore\n\e[0m");
        LOCK(&mlog3);
        fprintf(fl3, "SERVER : writen res writeFile_coda_stor errore\n");
        UNLOCK(&mlog3);
        fflush(stderr);

        return -1;
    }

    return 0;
}


int closeFile_coda_stor(CodaStorage_t *q, char *pathname, int fd) {

    if ((q == NULL) || (pathname == NULL)) {

        return EINVAL;
    }

    NodoStorage_t *item;

    LockQueue(q);
    if ((item = queue_s_find(q, pathname)) == NULL) {

        UnlockQueue(q);

        return ENOENT;
    }

    int esito;
    if ((esito = cancNodo_coda(item->opener_q, fd)) == -1) {

        UnlockQueue(q);

        return EPERM;
    } else if (esito == 1) {                                                    //è andata bene la delete (fd era l'ultimo opener)

        if (config.v > 2){
          printf("\e[0;36mSERVER : deleteNode OK (ed fd era l'ultimo opener)\n\e[0m");
          LOCK(&mlog3);
          fprintf(fl3, "SERVER : deleteNode OK (ed fd era l'ultimo opener)\n");
          UNLOCK(&mlog3);
        }
        item->opener_q = NULL;
    }

    if(item->locked && item->fd_locker == fd) {

        item->locked = false;
        item->fd_locker = -1;
        item->locker_can_write = false;
    }

    UnlockQueue(q);

    msg_risposta_t res;
    memset(&res, '\0', sizeof(msg_risposta_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {

        close(fd);
        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten res closeFile_coda_stor errore\n\e[0m");
        LOCK(&mlog3);
        fprintf(fl3, "SERVER : writen res closeFile_coda_stor errore\n");
        UNLOCK(&mlog3);
        fflush(stderr);

        return -1;
    }

    return 0;
}

int closeFdFiles_coda_stor(CodaStorage_t *q, int fd) {

    if (q == NULL) {

        return EINVAL;
    }

    LockQueue(q);

    NodoStorage_t *item = q->head;

    while(item != NULL) {

        int esito;
        if (item->opener_q == NULL) {

            item = item->next;
            continue;
        }
        if ((esito = cancNodo_coda(item->opener_q, fd)) == -1) {

            UnlockQueue(q);

            return EPERM;
        } else if (esito == 1) {                                                //è andata bene la delete (fd era l'ultimo opener)

            item->opener_q = NULL;
        }

        if(item->locked && item->fd_locker == fd) {

            item->locked = false;
            item->fd_locker = -1;
            item->locker_can_write = false;
        }

        item = item->next;
    }

    UnlockQueue(q);

    return 0;


}

int removeFile_coda_stor(CodaStorage_t *q, char *pathname, int fd) {

    if ((q == NULL) || (pathname == NULL)) {

        return EINVAL;
    }

    LockQueue(q);

    NodoStorage_t *temp = q->head;
    NodoStorage_t *prev = NULL;

    if (temp != NULL && (strcmp(temp->pathname, pathname) == 0)) {              // Se la testa conteneva fd

        q->head = temp->next;
        if (q->head == NULL)                                                    // cioè q->cur_numfiles è 1 (che stiamo togliendo adesso)

            q->tail = NULL;

    } else {
        while (temp != NULL && (strcmp(temp->pathname, pathname) != 0)){

            prev = temp;
            temp = temp->next;
        }

        if (temp != NULL) {                                                     // Se pathname non era presente

            if (q->tail == temp) q->tail = prev;

            prev->next = temp->next;
        }
    }

    if (temp != NULL) {

        if (!(temp->locked) || temp->fd_locker != fd) {

            UnlockQueue(q);

            return EPERM;
        }

        if (trova_coda(temp->opener_q, fd) == NULL) {

            UnlockQueue(q);

            return EPERM;
        }

        q->cur_numfiles -= 1;
        q->cur_usedstorage = q->cur_usedstorage - temp->len;

        free(temp->data);
        if (&temp->filecond)  pthread_cond_destroy(&temp->filecond);
        freeStorageNode(temp);
    }

    UnlockQueue(q);

    msg_risposta_t res;
    memset(&res, '\0', sizeof(msg_risposta_t));
    res.result = 0;
    res.datalen = 0;


    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {

        close(fd);
        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten res queue_s_deleteFile errore\n\e[0m");
        LOCK(&mlog3);
        fprintf(fl3, "SERVER : writen res queue_s_deleteFile errore\n");
        UNLOCK(&mlog3);
        fflush(stderr);

        return -1;
    }

    return 0;
}

void printListFiles_coda_stor(CodaStorage_t *q) {

    if (q == NULL) return;

    LockQueue(q);

    NodoStorage_t *curr = q->head;

    while(curr != NULL) {

        printf("\e[0;36mSERVER : %s\n\e[0m", curr->pathname);
        LOCK(&mlog3);
        fprintf(fl3, "SERVER : %s\n", curr->pathname);
        UNLOCK(&mlog3);
        fflush(stdout);

        curr = curr->next;

    }

    UnlockQueue(q);

}


unsigned long lung_coda_stor(CodaStorage_t *q) {

    LockQueue(q);
    unsigned long len = q->cur_numfiles;
    UnlockQueue(q);
    return len;
}


NodoStorage_t* queue_s_find(CodaStorage_t *q, char *pathname) {

    NodoStorage_t *curr = q->head;
    NodoStorage_t *found = NULL;

    while(found == NULL && curr != NULL) {

        if (config.v > 2){
          printf("\e[0;36mSERVER : queue_s_find, locked: %d, fd: %d, cwrite: %d, pathname: %s\n\e[0m", curr->locked, curr->fd_locker, curr->locker_can_write, curr->pathname);
          LOCK(&mlog3);
          fprintf(fl3, "SERVER : queue_s_find, locked: %d, fd: %d, cwrite: %d, pathname: %s\n", curr->locked, curr->fd_locker, curr->locker_can_write, curr->pathname);
          UNLOCK(&mlog3);
        }
        fflush(stdout);

        if (strcmp(curr->pathname, pathname) == 0) {

            found = curr;
        } else {

            curr = curr->next;
        }

    }

    return found;
}


NodoStorage_t *queue_s_pop(CodaStorage_t *q) {

    if (q == NULL) {

        errno= EINVAL;

        return NULL;
    }

    if (q->cur_numfiles == 0) {

        errno = ENOTTY;

        return NULL;
    }

    assert(q->head);
    NodoStorage_t *n  = (NodoStorage_t *)q->head;
    NodoStorage_t *poppedNode = q->head;
    q->head = q->head->next;
    poppedNode->next = NULL;

    if (q->cur_numfiles == 1)                                                   //il nodo da poppare è l'ultimo

        q->tail = q->head;

    q->cur_numfiles -= 1;
    q->cur_usedstorage = q->cur_usedstorage - n->len;

    return poppedNode;

}


int queue_s_popUntil(CodaStorage_t *q, int buf_len, int *num_poppedFiles, NodoStorage_t **poppedHead) {

    if (q == NULL) {

        return EINVAL;
    }

    if (q->cur_numfiles == 0) {

        return ENOTTY;
    }

    if (buf_len > q->storage_capacity) {

        return ENOMEM;
    }

    *poppedHead = q->head;
    NodoStorage_t *prec = NULL;

    bool cache_replacement = false;
    while ((q->cur_usedstorage + buf_len) > q->storage_capacity) {

        cache_replacement = true;
        q->cur_numfiles -= 1;
        q->cur_usedstorage = q->cur_usedstorage - q->head->len;

        *num_poppedFiles = (*num_poppedFiles) +1;

        prec = q->head;
        q->head = q->head->next;
    }
    if (prec != NULL) prec->next = NULL;

    if (q->head == NULL)

        q->tail = NULL;

    if (cache_replacement)

        q->replace_occur++;


    return 0;

}


int send_to_client_and_free(int fd, int num_files, NodoStorage_t *poppedH) {

    NodoStorage_t *temp = NULL;
    msg_risposta_t res;                                                           // messaggio risposta al client
    memset(&res, '\0', sizeof(msg_risposta_t));
    res.result = 0;
    res.datalen = num_files;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {                           // invio messaggio al client (quanti files riceverà)

        close(fd);
        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten res send_to_client_and_free errore\n\e[0m");
        LOCK(&mlog3);
        fprintf(fl3, "SERVER : writen res send_to_client_and_free errore\n");
        UNLOCK(&mlog3);
        fflush(stderr);
        queue_s_deleteNodes(poppedH);

        return -1;
    }

    if (config.v > 1){
      printf("\e[0;36mSERVER : Numero di files in espulsione: %d\n\e[0m", num_files);
      LOCK(&mlog3);
      fprintf(fl3, "SERVER : Numero di files in espulsione: %d\n", num_files);
      UNLOCK(&mlog3);
    }

    while(poppedH != NULL) {

        temp = poppedH;
        res.datalen = temp->len;
        strcpy(res.pathname, temp->pathname);

        if (config.v > 1){
          printf("\e[0;36mSERVER : Espulso file %s, di lunghezza %d\n\e[0m", temp->pathname, temp->len);
          LOCK(&mlog3);
          fprintf(fl3, "SERVER : Espulso file %s, di lunghezza %d\n", temp->pathname, temp->len);
          UNLOCK(&mlog3);
        }
        fflush(stdout);

        if (writen(fd, &res, sizeof(res)) != sizeof(res)) {

            close(fd);
            fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten res send_to_client_and_free errore\n\e[0m");
            LOCK(&mlog3);
            fprintf(fl3, "SERVER : writen res send_to_client_and_free errore\n");
            UNLOCK(&mlog3);
            fflush(stderr);
            queue_s_deleteNodes(poppedH);

            return -1;
        }

        if (res.datalen > 0) {

            if (writen(fd, temp->data, res.datalen) != res.datalen) {

                close(fd);
                fprintf(stderr, "\e[0;36mSERVER : \e[0;31mwriten data send_to_client_and_free errore\n\e[0m");
                LOCK(&mlog3);
                fprintf(fl3, "SERVER : writen data send_to_client_and_free errore\n");
                UNLOCK(&mlog3);
                fflush(stderr);
                queue_s_deleteNodes(poppedH);

                return -1;
            }

            free(temp->data);

        }



        num_files--;
        poppedH = poppedH->next;

        if (&temp->filecond)  pthread_cond_destroy(&temp->filecond);
        freeStorageNode(temp);

    }

    assert(num_files == 0);

    return 0;
}
