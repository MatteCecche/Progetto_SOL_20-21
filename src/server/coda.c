#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "../../includes/util.h"
#include "../../includes/coda.h"
#include "../../includes/config.h"

extern struct config_struct config;





// --------------------------------------- funzioni di utilità ---------------------------------------- //



static inline Nodo_t  *allocNodo()                  { return malloc(sizeof(Nodo_t));  }
static inline Coda_t *allocCoda()                 { return malloc(sizeof(Coda_t)); }
static inline void freeNode(Nodo_t *node)           { free((void*)node); }
static inline void LockQueue(Coda_t *q)            { LOCK(&q->qlock);   }
static inline void UnlockQueue(Coda_t *q)          { UNLOCK(&q->qlock); }
static inline void UnlockQueueAndWait(Coda_t *q)   { WAIT(&q->qcond, &q->qlock); }
static inline void UnlockQueueAndSignal(Coda_t *q) { SIGNAL(&q->qcond); UNLOCK(&q->qlock); }


// --------------------------------------- interfaccia della coda -------------------------------------- //

Coda_t *init_coda() {

    Coda_t *q = allocCoda();

    if (!q) return NULL;

    q->head = NULL;
    q->tail = NULL;
    q->qlen = 0;

    if (pthread_mutex_init(&q->qlock, NULL) != 0) {

        perror("SERVER : ERRORE mutex init");

        return NULL;
    }

    if (pthread_cond_init(&q->qcond, NULL) != 0) {

        perror("SERVER : ERRORE mutex cond");

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

    if (q == NULL) { errno= EINVAL; return -1;}
    Nodo_t *n = allocNodo();
    if (!n) return -1;
    n->data = data;
    n->next = NULL;

    LockQueue(q);

    if (q->qlen == 0) {

        q->head = n;
        q->tail = n;
    } else {

        q->tail->next = n;
        q->tail = n;
    }

    q->qlen += 1;
    UnlockQueueAndSignal(q);

    return 0;
}


int estrai_coda(Coda_t *q) {

    if (q == NULL) {

        errno= EINVAL;

        return -1;
    }

    LockQueue(q);
    while(q->qlen == 0) {

      UnlockQueueAndWait(q);
    }

    Nodo_t *n  = (Nodo_t *)q->head;
    int data = q->head->data;
    q->head    = q->head->next;
    q->qlen   -= 1;

    UnlockQueue(q);
    freeNode(n);

    return data;

}


Nodo_t* trova_coda(Coda_t *q, int fd) {

    if (q == NULL) {

        errno= EINVAL;

        return NULL;
    }
    LockQueue(q);
    Nodo_t *curr = q->head;
    Nodo_t *found = NULL;

    if (config.v > 2) printf("SERVER : trova_coda fd:%d, in opener_q\n", fd);

    while(found == NULL && curr != NULL) {
        if (config.v > 2) printf("SERVER : fd: %d\n", curr->data);

        if (fd == curr->data) {

            found = curr;
        } else {

            curr = curr->next;
        }
    }

    UnlockQueue(q);

    return found;
}


int cancNodo_coda(Coda_t *q, int fd) {

    if (q == NULL) {

        errno = EINVAL;

        return -1;
    }

    LockQueue(q);

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
            UnlockQueue(q);
        }

        return 0;

    } else {

        while (temp != NULL && temp->data != fd) {

            prev = temp;
            temp = temp->next;
        }

        if (temp == NULL) {                       // Se fd non era presente

            UnlockQueue(q);

            return 0;
        }

        prev->next = temp->next;

        freeNode(temp);
    }

    q->qlen -= 1;

    UnlockQueue(q);

    return 0;
}



unsigned long lung_coda(Coda_t *q) {

    LockQueue(q);
    unsigned long len = q->qlen;
    UnlockQueue(q);

    return len;
}
