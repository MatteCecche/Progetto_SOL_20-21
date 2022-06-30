#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include "util.h"
#include "codastorage.h"
#include "common_def.h"
#include "common_funcs.h"
#include "coda.h"
#include "config.h"

extern struct config_struct config;
extern server_status status;


// Restituisce il nodo identificato da pathname

NodoStorage* trova_coda_s(CodaStorage *q, char *pathname);

// Estrae dalla coda q un numero di files adeguato per permettere l'inserimento di un file di buf_len bytes,
//  non superando la capacità dello storage in bytes
//  (da chiamare con lock (su q))

int queue_s_popUntil(CodaStorage *q, int buf_len, int *num_poppedFiles, NodoStorage **poppedHead);

// Estrae dalla coda q un nodo (file)
 //  (da chiamare con lock)

NodoStorage *queue_s_pop(CodaStorage *q);

// Comunica al client sul socket fd, i files della coda poppedH, e libera memoria

int send_to_client_and_free(int fd, int num_files, NodoStorage *poppedH);


CodaStorage *init_coda_s(int limit_num_files, unsigned long storage_capacity) {

  CodaStorage *q = malloc(sizeof(CodaStorage));;

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
	    perror("SERVER: mutex init");
	    return NULL;
    }

    return q;
}

// da chiamare quando server "muore" (prima della delete)

void broadcast_coda_s(CodaStorage *q) {

  if (q == NULL) return;

  NodoStorage *temp = q->head;
  while(temp != NULL) {
    if (temp->locked) {
      BCAST(&temp->filecond);
    }
    temp = temp->next;
    }

}

// da chiamare quando server "muore"

void canc_coda_s(CodaStorage *q) {

  if (q == NULL) return;

  while(q->head != NULL) {
    NodoStorage *p = (NodoStorage*)q->head;
    q->head = q->head->next;

    if (p->len > 0) free(p->data);
    if (&p->filecond)  pthread_cond_destroy(&p->filecond);
    canc_coda(p->opener_q);
    free((void*)p);
    }

    if (&q->qlock)  pthread_mutex_destroy(&q->qlock);


    free(q);
}

void canc_nodi_coda_s (NodoStorage *head) {
    while(head != NULL) {
        NodoStorage *p = (NodoStorage*) head;
        head = head->next;

        if (p->len > 0) free(p->data);
        if (&p->filecond)  pthread_cond_destroy(&p->filecond);
        canc_coda(head->opener_q);
        free((void*)head);
    }

}

int ins_coda_s(CodaStorage *q, char *pathname, bool locked, int fd) {

    NodoStorage *poppedNode = NULL;

    if ((q == NULL) || (pathname == NULL)) {
        return EINVAL;
    }

    if (config.v > 2) printf("Server : ins_coda_s, fd: %d, pathname: %s\n", fd, pathname);
    fflush(stdout);

    NodoStorage *n = malloc(sizeof(NodoStorage));();

    if (!n) return ENOMEM;

    memset(n, '\0', sizeof(NodoStorage));

    if (pthread_cond_init(&n->filecond, NULL) != 0) {
	    perror("Errore : mutex cond");
        return EAGAIN;
    }

    strncpy(n->pathname, pathname, PATH_MAX);
    n->locked = locked;
    n->fd_locker = (locked) ? fd : -1;
    n->locker_can_write = locked;

    n->opener_q = queue_init();
    if (!n->opener_q)
    {
        fprintf(stderr, "Errore fd: %d, initQueue fallita\n", fd);
        if (&n->filecond)  pthread_cond_destroy(&n->filecond);
        canc_coda(n->opener_q);
        free((void*)n);
        return EAGAIN;
    }
    if (queue_push(n->opener_q, fd) != 0) {
        fprintf(stderr, "Errore fd: %d, push opener_q fallita\n", fd);
        if (&n->filecond)  pthread_cond_destroy(&n->filecond);
        canc_coda(n->opener_q);
        free((void*)n);
        return EAGAIN;
    }

    n->next = NULL;


    LOCK(&q->qlock);(q);
    if (trova_coda_s(q, pathname) != NULL) {
        if (&n->filecond)  pthread_cond_destroy(&n->filecond);
        canc_coda(n->opener_q);
        free((void*)n);

        UNLOCK(&q->qlock);(q);

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
        if (config.v > 2) printf("Errore : Limite di files raggiunto\n");
        if((poppedNode = queue_s_pop(q)) == NULL) {
            UNLOCK(&q->qlock);
            return errno;
        }

        q->replace_occur++;
        q->cur_numfiles += 1;
        UNLOCK(&q->qlock);(q);

        if (send_to_client_and_free(fd, 1, poppedNode) == -1) {
            return -1;
        }

        return 0;
    } else {
        if (q->max_num_files == q->cur_numfiles) q->max_num_files++;
    }
    q->cur_numfiles += 1;

    UNLOCK(&q->qlock);(q);

    msg_response_t res;
    memset(&res, '\0', sizeof(msg_response_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd);
        fprintf(stderr, "Errore : writen res coda_s_openFile errore\n");
        fflush(stdout);
        return -1;
    }

    return 0;
}

int aggiornaOpeners_coda_s(CodaStorage *q, char *pathname, bool locked, int fd) {
    if ((q == NULL) || (pathname == NULL)) {
        return EINVAL;
    }

    NodoStorage *item;

    LOCK(&q->qlock);(q);
    if ((item = trova_coda_s(q, pathname)) == NULL) {
        UNLOCK(&q->qlock);
        return ENOENT;
    }

    while (item->locked && item->fd_locker != fd && status != CLOSED) {
        if (config.v > 2) printf("SERVER: coda_s_openFile, fd: %d, file: %s locked, aspetto...\n", fd, pathname);
        WAIT(&item->filecond, &q->qlock);
    }
    if (status == CLOSED) {
        return ENETDOWN;
    }
    if (item->locked) { //&& item->fd_locker == fd
        if (!locked) { //non lo voglio lockare, unlocko
            item->locked = false;
            item->fd_locker = -1;
        }
        item->locker_can_write = false;
    } else {
        if (locked) {
            item->locked = true;
            item->fd_locker = fd;
            item->locker_can_write = false; //superfluo
        }
    }

    if(item->opener_q == NULL) {
        item->opener_q = queue_init();
        if (!q)
        {
            fprintf(stderr, "Errore fd: %d, initQueue fallita\n", fd);
            UNLOCK(&q->qlock);
            return EAGAIN;
        }
        if (queue_push(item->opener_q, fd) != 0) {
            fprintf(stderr, "Errore fd: %d, push opener_q fallita\n", fd);
            UNLOCK(&q->qlock);
            return EAGAIN;
        }
    } else if (queue_find(item->opener_q, fd) == NULL) {
        if(queue_push(item->opener_q, fd) != 0) {
            fprintf(stderr, "Errore fd: %d, push opener_q fallita\n", fd);
            UNLOCK(&q->qlock);
            return EAGAIN;
        }
    }

    UNLOCK(&q->qlock);(q);

    msg_response_t res;
    memset(&res, '\0', sizeof(msg_response_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd);
        fprintf(stderr, "Errore : writen res coda_s_openFile errore\n");
        return -1;
    }

    return 0;
}


int readFile_coda_s(CodaStorage *q, char *pathname, int fd) {
    if ((q == NULL) || (pathname == NULL)) {
        return EINVAL;
    }

    NodoStorage *item;

    LOCK(&q->qlock);(q);
    if ((item = trova_coda_s(q, pathname)) == NULL) {
        UNLOCK(&q->qlock);
        return ENOENT;
    }

    if (item->locked && item->fd_locker != fd) {
        UNLOCK(&q->qlock);
        return EPERM;
    }

    if (item->locked) { //&& item->fd_locker == fd
        item->locker_can_write = false;
    }

    if (queue_find(item->opener_q, fd) == NULL) {
        UNLOCK(&q->qlock);
        return EPERM;
    }

    msg_response_t res; // messaggio risposta al client
    memset(&res, '\0', sizeof(msg_response_t));
    res.result = 0;
    res.datalen = item->len;
    strcpy(res.pathname, item->pathname); //superfluo

    //invio messaggio al client
    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd);
        fprintf(stderr, "Errore : writen res readFile_coda_s errore\n");

        UNLOCK(&q->qlock);
        return -1;
    }

    if (writen(fd, item->data, res.datalen) != res.datalen) {
        close(fd);
        if (config.v > 1) printf("Errore : writen data readFile_coda_s errore\n");
        fflush(stdout);

        UNLOCK(&q->qlock);
        return -1;
    }

    if (config.v > 1) printf("Errore : Letto (ed inviato) file %s, di lunghezza %d\n", item->pathname, item->len);
    fflush(stdout);

    UNLOCK(&q->qlock);

    return 0;
}

int readNFile_coda_s(CodaStorage *q, char *pathname, int fd, int n) {
    msg_response_t res; // messaggio risposta al client
    memset(&res, '\0', sizeof(msg_response_t));
    res.result = 0;

    if ((q == NULL) || (pathname == NULL)) {
        return EINVAL;
    }

    LOCK(&q->qlock);(q);

    if (q->cur_numfiles < n || n <= 0) res.datalen = q->cur_numfiles;
    else res.datalen = n;

    if (config.v > 1) printf("Errore : Numero di files che verranno inviati: %d\n", res.datalen);

    // invio messaggio al client
    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd);
        fprintf(stderr, "Errore : writen res readFile_coda_s errore\n");
        fflush(stderr);

        UNLOCK(&q->qlock);
        return -1;
    }

    NodoStorage *temp = q->head;
    int num = res.datalen;
    int i = 1;
    while (i <= num) {
        res.datalen = temp->len;
        strcpy(res.pathname, temp->pathname);

        //invio messaggio al client
        if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
            close(fd);
            fprintf(stderr, "Errore : writen res readFile_coda_s errore\n");
            fflush(stderr);

            UNLOCK(&q->qlock);
            return -1;
        }

        if (writen(fd, temp->data, res.datalen) != res.datalen) {
            close(fd);
            fprintf(stderr, "Errore : writen data readFile_coda_s errore\n");
            fflush(stderr);

            UNLOCK(&q->qlock);
            return -1;
        }


        if (config.v > 1) printf("Errore : Letto (ed inviato) file %s, di lunghezza %d\n", temp->pathname, temp->len);
        fflush(stdout);

        temp = temp->next;
        i++;

    }

    UNLOCK(&q->qlock);

    return 0;
}


int writeFile_coda_s(CodaStorage *q, char *pathname, int fd, void *buf, int buf_len) {
    if ((q == NULL) || (pathname == NULL) || buf == NULL) {
        return EINVAL;
    }

    NodoStorage *item;

    LOCK(&q->qlock);(q);
    if ((item = trova_coda_s(q, pathname)) == NULL) {
        UNLOCK(&q->qlock);
        return ENOENT;
    }

    if (!(item->locked) || item->fd_locker != fd || !(item->locker_can_write)) {
        UNLOCK(&q->qlock);
        return EPERM;
    }

    if (queue_find(item->opener_q, fd) == NULL) {
        UNLOCK(&q->qlock);
        return EPERM;
    }

    if (buf_len > q->storage_capacity) {
        UNLOCK(&q->qlock);
        return ENOMEM;
    }

    NodoStorage *poppedHead = NULL;
    int num_poppedFiles = 0;
    int result = queue_s_popUntil(q, buf_len, &num_poppedFiles, &poppedHead);
    if (result != 0) {
        UNLOCK(&q->qlock);
        return result;
    }

    void *tmp = malloc(buf_len);
    if (tmp == NULL) {
        UNLOCK(&q->qlock);

        fprintf(stderr, "Errore fd: %d, queue_s_writeToFile malloc fallita\n", fd);
        return ENOMEM;
    }

    memcpy(tmp, buf, buf_len);

    item->len = buf_len;
    item->data = tmp;

    if (config.v > 1) printf("Errore : Scritto il file %s, per una lunghezza di %d\n", item->pathname, item->len);
    fflush(stdout);

    q->cur_usedstorage += item->len;

    if (q->max_used_storage < q->cur_usedstorage) q->max_used_storage = q->cur_usedstorage;

    item->locker_can_write = false;

    UNLOCK(&q->qlock);

    if (num_poppedFiles != 0) {
        if (send_to_client_and_free(fd, num_poppedFiles, poppedHead) == -1) {
            return -1;
        }

        return 0;
    }

    msg_response_t res;
    memset(&res, '\0', sizeof(msg_response_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd);
        fprintf(stderr, "Errore : writen res writeFile_coda_s errore\n");
        fflush(stderr);
        return -1;
    }

    return 0;
}


int appendToFile_coda_s(CodaStorage *q, char *pathname, int fd, void *buf, int buf_len) {

    if ((q == NULL) || (pathname == NULL)) {
        return EINVAL;
    }

    NodoStorage *item = NULL;

    LOCK(&q->qlock);(q);
    if ((item = trova_coda_s(q, pathname)) == NULL) {
        UNLOCK(&q->qlock);
        return ENOENT;
    }

    if (queue_find(item->opener_q, fd) == NULL) {
        if (config.v > 2) printf("SERVER : item->opener_q == NULL ? %d\n", (item->opener_q == NULL));
        fflush(stdout);
        UNLOCK(&q->qlock);(q);
        return EPERM;
    }

    // HO SOLO GUARDATO DI ESSERE UN OPENER
    // L'ATOMICITA' è garantita da LOCK(&q->qlock);

    NodoStorage *poppedHead = NULL;
    int num_poppedFiles = 0;
    int result = queue_s_popUntil(q, buf_len, &num_poppedFiles, &poppedHead);
    if (result != 0) {
        UNLOCK(&q->qlock);
        return result;
    }

    void *tmp = realloc(item->data, (item->len) + buf_len);
    if (tmp == NULL) {
        UNLOCK(&q->qlock);

        fprintf(stderr, "Errore fd: %d, appendToFile_coda_s realloc fallita\n", fd);
        return ENOMEM;
    }

    memcpy(&tmp[item->len], buf, buf_len);

    item->data = tmp;
    item->len += buf_len;

    if (config.v > 1) printf("SERVER : Append sul file %s, lunghezza attuale di %d\n", item->pathname, item->len);
    fflush(stdout);

    q->cur_usedstorage += buf_len;

    if (q->max_used_storage < q->cur_usedstorage) q->max_used_storage = q->cur_usedstorage;

    UNLOCK(&q->qlock);

    if (num_poppedFiles != 0) {
        if (send_to_client_and_free(fd, num_poppedFiles, poppedHead) == -1) {
            return -1;
        }

        return 0;
    }

    msg_response_t res;
    memset(&res, '\0', sizeof(msg_response_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd);
        fprintf(stderr, "Errore : writen res appendToFile_coda_s errore\n");
        fflush(stderr);
        return -1;
    }

    return 0;
}

int lockFile_coda_s(CodaStorage *q, char *pathname, int fd) {
    if ((q == NULL) || (pathname == NULL)) {
        return EINVAL;
    }

    NodoStorage *item;

    LOCK(&q->qlock);(q);
    if ((item = trova_coda_s(q, pathname)) == NULL) {
        UNLOCK(&q->qlock);
        return ENOENT;
    }

    while (item->locked && item->fd_locker != fd && status != CLOSED) {
        if (config.v > 2) printf("SERVER : lockFile_coda_s, fd: %d, file: %s locked, aspetto...\n", fd, pathname);
        WAIT(&q->filecond, &item->qlock);(q, item);
    }
    if (status == CLOSED) {
        return ENETDOWN;
    }

    if (queue_find(item->opener_q, fd) == NULL) {
        UNLOCK(&q->qlock);
        return EPERM;
    }

    item->locked = true;
    item->fd_locker = fd;
    item->locker_can_write = false;

    UNLOCK(&q->qlock);

    msg_response_t res;
    memset(&res, '\0', sizeof(msg_response_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd);
        fprintf(stderr, "Errore : writen res lockFile_coda_s errore\n");
        fflush(stderr);
        return -1;
    }

    return 0;
}


int unlockFile_coda_s(CodaStorage *q, char *pathname, int fd) {
    if ((q == NULL) || (pathname == NULL)) {
        return EINVAL;
    }

    NodoStorage *item;

    LOCK(&q->qlock);(q);
    if ((item = trova_coda_s(q, pathname)) == NULL) {
        UNLOCK(&q->qlock);
        return ENOENT;
    }

    if (!(item->locked) || item->fd_locker != fd) {
        UNLOCK(&q->qlock);
        return EPERM;
    }

    if (queue_find(item->opener_q, fd) == NULL) {
        UNLOCK(&q->qlock);
        return EPERM;
    }

    item->locked = false;
    item->fd_locker = -1;
    item->locker_can_write = false;

    SIGNAL(&item->filecond); UNLOCK(&q->qlock);

    msg_response_t res;
    memset(&res, '\0', sizeof(msg_response_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd);
        fprintf(stderr, "Errore : writen res writeFile_coda_s errore\n");
        fflush(stderr);
        return -1;
    }

    return 0;
}


int closeFile_coda_s(CodaStorage *q, char *pathname, int fd) {
    if ((q == NULL) || (pathname == NULL)) {
        return EINVAL;
    }

    NodoStorage *item;

    LOCK(&q->qlock);(q);
    if ((item = trova_coda_s(q, pathname)) == NULL) {
        UNLOCK(&q->qlock);
        return ENOENT;
    }

    int esito;
    if ((esito = queue_deleteNode(item->opener_q, fd)) == -1) {
        UNLOCK(&q->qlock);
        return EPERM;
    } else if (esito == 1) { //è andata bene la delete (fd era l'ultimo opener)
        if (config.v > 2) printf("SERVER : deleteNode OK (ed fd era l'ultimo opener)\n");
        item->opener_q = NULL;
    }

    if(item->locked && item->fd_locker == fd) {
        item->locked = false;
        item->fd_locker = -1;
        item->locker_can_write = false;
    }

    UNLOCK(&q->qlock);

    msg_response_t res;
    memset(&res, '\0', sizeof(msg_response_t));
    res.result = 0;
    res.datalen = 0;

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd);
        fprintf(stderr, "Errore : writen res closeFile_coda_s errore\n");
        fflush(stderr);
        return -1;
    }

    return 0;
}

int closeFdFile_coda_s(CodaStorage *q, int fd) {
    if (q == NULL) {
        return EINVAL;
    }

    LOCK(&q->qlock);

    NodoStorage *item = q->head;

    while(item != NULL) {
        int esito;
        if (item->opener_q == NULL) {
            item = item->next;
            continue;
        }
        if ((esito = queue_deleteNode(item->opener_q, fd)) == -1) {
            UNLOCK(&q->qlock);
            return EPERM;
        } else if (esito == 1) { //è andata bene la delete (fd era l'ultimo opener)
            item->opener_q = NULL;
        }

        if(item->locked && item->fd_locker == fd) {
            item->locked = false;
            item->fd_locker = -1;
            item->locker_can_write = false;
        }

        item = item->next;
    }

    UNLOCK(&q->qlock);

    return 0;


}

int removeFile_coda_s(CodaStorage *q, char *pathname, int fd) {
    if ((q == NULL) || (pathname == NULL)) {
        return EINVAL;
    }

    LOCK(&q->qlock);

    NodoStorage *temp = q->head;
    NodoStorage *prev = NULL;

    // Se la testa conteneva fd
    if (temp != NULL && (strcmp(temp->pathname, pathname) == 0)) {
        q->head = temp->next;
        if (q->head == NULL) // cioè q->cur_numfiles è 1 (che stiamo togliendo adesso)
            q->tail = NULL;

    } else {
        while (temp != NULL && (strcmp(temp->pathname, pathname) != 0)){
            prev = temp;
            temp = temp->next;
        }

        // Se pathname non era presente
        if (temp != NULL) {
            if (q->tail == temp) q->tail = prev;

            prev->next = temp->next;
        }
    }

    if (temp != NULL) {
        if (!(temp->locked) || temp->fd_locker != fd) {
            UNLOCK(&q->qlock);
            return EPERM;
        }

        if (queue_find(temp->opener_q, fd) == NULL) {
            UNLOCK(&q->qlock);
            return EPERM;
        }

        q->cur_numfiles -= 1;
        q->cur_usedstorage = q->cur_usedstorage - temp->len;

        free(temp->data);
        if (&temp->filecond)  pthread_cond_destroy(&temp->filecond);
        canc_coda(temp->opener_q);
        free((void*)temp);
    }

    UNLOCK(&q->qlock);(q);

    msg_response_t res;
    memset(&res, '\0', sizeof(msg_response_t));
    res.result = 0;
    res.datalen = 0;


    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd);
        fprintf(stderr, "Errore : writen res queue_s_deleteFile errore\n");
        fflush(stderr);
        return -1;
    }
    return 0;
}

void stampalistaFile_coda_s(CodaStorage *q) {

    if (q == NULL) return;

    LOCK(&q->qlock);(q);

    NodoStorage *curr = q->head;

    while(curr != NULL) {
        printf("SERVER: %s\n", curr->pathname);
        fflush(stdout);

        curr = curr->next;

    }

    UNLOCK(&q->qlock);

}


// NOTA: in questa funzione si puo' accedere a q->cur_numfiles NON in mutua esclusione
//       ovviamente il rischio e' quello di leggere un valore non aggiornato, ma nel
//       caso di piu' produttori e consumatori la lunghezza della coda per un thread
//       e' comunque un valore "non affidabile" se non all'interno di una transazione
//       in cui le varie operazioni sono tutte eseguite in mutua esclusione.
unsigned long lung_coda_s(CodaStorage *q) {

    LOCK(&q->qlock);
    unsigned long len = q->cur_numfiles;
    UNLOCK(&q->qlock);
    return len;
}


NodoStorage* trova_coda_s(CodaStorage *q, char *pathname) {

    NodoStorage *curr = q->head;
    NodoStorage *found = NULL;

    while(found == NULL && curr != NULL) {
        if (config.v > 2) printf("SERVER: trova_coda_s, locked: %d, fd: %d, cwrite: %d, pathname: %s\n", curr->locked, curr->fd_locker, curr->locker_can_write, curr->pathname);
        fflush(stdout);

        if (strcmp(curr->pathname, pathname) == 0) {
            found = curr;
        } else {
            curr = curr->next;
        }

    }

    return found;
}


NodoStorage *queue_s_pop(CodaStorage *q) {
    if (q == NULL) {
        errno= EINVAL;
        return NULL;
    }

    if (q->cur_numfiles == 0) {
	    errno = ENOTTY;
        return NULL;
    }

    assert(q->head);
    NodoStorage *n  = (NodoStorage *)q->head;
    NodoStorage *poppedNode = q->head;
    q->head = q->head->next;
    poppedNode->next = NULL;

    if (q->cur_numfiles == 1) //il nodo da poppare è l'ultimo
        q->tail = q->head;

    q->cur_numfiles -= 1;
    q->cur_usedstorage = q->cur_usedstorage - n->len;

    return poppedNode;

}


int queue_s_popUntil(CodaStorage *q, int buf_len, int *num_poppedFiles, NodoStorage **poppedHead) {
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
    NodoStorage *prec = NULL;

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


int send_to_client_and_free(int fd, int num_files, NodoStorage *poppedH) {
    NodoStorage *temp = NULL;
    msg_response_t res; // messaggio risposta al client
    memset(&res, '\0', sizeof(msg_response_t));
    res.result = 0;
    res.datalen = num_files;

    // invio messaggio al client (quanti files riceverà)
    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
        close(fd);
        fprintf(stderr, "Errore : writen res send_to_client_and_free errore\n");
        fflush(stderr);
        canc_nodi_coda_s(poppedH);
        return -1;
    }

    if (config.v > 1) printf("Errore : Numero di files in espulsione: %d\n", num_files);

    while(poppedH != NULL) {
        temp = poppedH;
        res.datalen = temp->len;
        strcpy(res.pathname, temp->pathname);

        if (config.v > 1) printf("Errore : Espulso file %s, di lunghezza %d\n", temp->pathname, temp->len);
        fflush(stdout);

        if (writen(fd, &res, sizeof(res)) != sizeof(res)) {
            close(fd);
            fprintf(stderr, "Errore : writen res send_to_client_and_free errore\n");
            fflush(stderr);
            canc_nodi_coda_s(poppedH);
            return -1;
        }

        if (res.datalen > 0) {
            if (writen(fd, temp->data, res.datalen) != res.datalen) {
                close(fd);
                fprintf(stderr, "Errore : writen data send_to_client_and_free errore\n");
                fflush(stderr);
                canc_nodi_coda_s(poppedH);
                return -1;
            }

            free(temp->data);

        }



        num_files--;
        poppedH = poppedH->next;

        if (&temp->filecond)  pthread_cond_destroy(&temp->filecond);
        canc_coda(temp->opener_q);
        free((void*)temp);

    }

    assert(num_files == 0);

    return 0;
}
