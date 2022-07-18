#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "../../includes/util.h"
#include "../../includes/coda.h"
#include "../../includes/config.h"

// ------------------------------------------------------------------ variabili globali -------------------------------------------- //


extern struct config_struct config;
FILE *fl4;
pthread_mutex_t mlog4;



// ----------------------------------------------------- funzioni di utilità ------------------------------------------------------ //



static inline Nodo_t  *allocNodo(){

  return malloc(sizeof(Nodo_t));

}

static inline Coda_t *allocCoda(){

  return malloc(sizeof(Coda_t));

}

static inline void freeNode(Nodo_t *node){

  free((void*)node);

}

static inline void LockCoda(Coda_t *q){

  LOCK(&q->qlock);

}

static inline void UnlockCoda(Coda_t *q){

  UNLOCK(&q->qlock);

}

static inline void UnlockCodaEAspetta(Coda_t *q){

  WAIT(&q->qcond, &q->qlock);

}

static inline void UnlockCodaESignal(Coda_t *q){

  SIGNAL(&q->qcond);
  UNLOCK(&q->qlock);

}


// ------------------------------------------------- interfaccia della coda ---------------------------------------------------------- //



Coda_t *init_coda(FILE *l, pthread_mutex_t ml) {

    fl4 = l;
    mlog4 = ml;

    Coda_t *q = allocCoda();

    if (!q) return NULL;

    q->head = NULL;
    q->tail = NULL;
    q->qlen = 0;

    if (pthread_mutex_init(&q->qlock, NULL) != 0) {

        perror("\e[0;36mSERVER : \e[0;31mERRORE mutex init\e[0m");
        LOCK(&mlog4);
        fprintf(fl4, "SERVER : ERRORE mutex init");
        UNLOCK(&mlog4);

        return NULL;
    }

    if (pthread_cond_init(&q->qcond, NULL) != 0) {

        perror("\e[0;36mSERVER : \e[0;31mERRORE mutex cond\e[0m");
        LOCK(&mlog4);
        fprintf(fl4, "SERVER : ERRORE mutex cond");
        UNLOCK(&mlog4);

        if (&q->qlock) pthread_mutex_destroy(&q->qlock);

        return NULL;
    }

    return q;
}


void canc_coda(Coda_t *q) {

    if (q == NULL) return;

    while(q->head != NULL) {

      Nodo_t *p = (Nodo_t*)q->head;
	    q->head = q->head->next;
	    freeNode(p);
    }

    if (&q->qlock)  pthread_mutex_destroy(&q->qlock);
    if (&q->qcond)  pthread_cond_destroy(&q->qcond);
    free(q);
}


int ins_coda(Coda_t *q, int data) {

    if (q == NULL) {

      errno= EINVAL; 
      return -1;
    }
    Nodo_t *n = allocNodo();
    if (!n) return -1;
    n->data = data;
    n->next = NULL;

    LockCoda(q);

    if (q->qlen == 0) {

        q->head = n;
        q->tail = n;
    } else {

        q->tail->next = n;
        q->tail = n;
    }

    q->qlen += 1;
    UnlockCodaESignal(q);

    return 0;
}


int estrai_coda(Coda_t *q) {

    if (q == NULL) {

        errno= EINVAL;

        return -1;
    }

    LockCoda(q);
    while(q->qlen == 0) {

      UnlockCodaEAspetta(q);
    }

    Nodo_t *n  = (Nodo_t *)q->head;
    int data = q->head->data;
    q->head    = q->head->next;
    q->qlen   -= 1;

    UnlockCoda(q);
    freeNode(n);

    return data;

}


Nodo_t* trova_coda(Coda_t *q, int fd) {

    if (q == NULL) {

        errno= EINVAL;

        return NULL;
    }
    LockCoda(q);
    Nodo_t *curr = q->head;
    Nodo_t *found = NULL;

    while(found == NULL && curr != NULL) {


        if (fd == curr->data) {

            found = curr;
        } else {

            curr = curr->next;
        }
    }

    UnlockCoda(q);

    return found;
}


int cancNodo_coda(Coda_t *q, int fd) {

    if (q == NULL) {

        errno = EINVAL;

        return -1;
    }

    LockCoda(q);

    Nodo_t* temp = q->head;
    Nodo_t* prev = NULL;

    if (temp != NULL && temp->data == fd){          // Se la testa conteneva fd

        q->head = temp->next;

        freeNode(temp);
        q->qlen   -= 1;
        if (q->qlen == 0) {

            if (&q->qlock)  pthread_mutex_destroy(&q->qlock);
            if (&q->qcond)  pthread_cond_destroy(&q->qcond);
            free(q);

            return 1;

        } else {

            q->qlen -= 1;
            UnlockCoda(q);
        }

        return 0;

    } else {

        while (temp != NULL && temp->data != fd) {

            prev = temp;
            temp = temp->next;
        }

        if (temp == NULL) {                       // Se fd non era presente

            UnlockCoda(q);

            return 0;
        }

        prev->next = temp->next;

        freeNode(temp);
    }

    q->qlen -= 1;

    UnlockCoda(q);

    return 0;
}



unsigned long lung_coda(Coda_t *q) {

    LockCoda(q);
    unsigned long len = q->qlen;
    UnlockCoda(q);

    return len;
}
