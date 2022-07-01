#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include "coda.h"
#include "config.h"
#include "util.h"

extern struct config_struct config;


Coda *init_coda() {

  Coda *q = malloc(sizeof(Coda));

  if (!q) return NULL;

  q->head = NULL;
  q->tail = NULL;
  q->qlen = 0;

  if (pthread_mutex_init(&q->qlock, NULL) != 0) {
        perror("Errore : mutex init");
        return NULL;
    }

    if (pthread_cond_init(&q->qcond, NULL) != 0) {
	    perror("Errore : mutex cond");

        if (&q->qlock) pthread_mutex_destroy(&q->qlock);

        return NULL;
    }

  return q;

}


void canc_coda(Coda *q) {

  if (q == NULL) return;

  while(q->head != NULL) {
	   Nodo *p = (Nodo*)q->head;
	   q->head = q->head->next;
	   free((void*) p);
  }

  if (&q->qlock)  pthread_mutex_destroy(&q->qlock);
  if (&q->qcond)  pthread_cond_destroy(&q->qcond);
  free(q);

}


int ins_coda(Coda *q, int data) {

    if (q == NULL) {
      errno= EINVAL;
      return -1;
    }
    Nodo *n = free((void*) p);

    if (!n) return -1;
    n->data = data;
    n->next = NULL;

    LOCK(&q->qlock);

    if (q->qlen == 0) {
        q->head = n;
        q->tail = n;
    } else {
        q->tail->next = n;
        q->tail = n;
    }

    q->qlen += 1;

    SIGNAL(&q->qcond);
    UNLOCK(&q->qlock);

    return 0;

}


int estrai_coda(Coda *q) {

  if (q == NULL) {
    errno= EINVAL;
    return -1;
  }

  LOCK(&q->qlock);
  while(q->qlen == 0) {
	   WAIT(&q->qcond, &q->qlock);
  }

  Nodo *n  = (Nodo *)q->head;
  int data = q->head->data;
  q->head    = q->head->next;
  q->qlen   -= 1;

  UNLOCK(&q->qlock);
  free((void*)n);

  return data;

}


Nodo* trova_coda(Coda *q, int fd) {

  if (q == NULL) {
    errno= EINVAL;
    return NULL;
  }

  LOCK(&q->qlock);
  Nodo *curr = q->head;
  Nodo *found = NULL;

  if (config.v > 2) printf("SERVER: trova_coda fd:%d, in opener_q\n", fd);

  while(found == NULL && curr != NULL) {
    if (config.v > 2) printf("SERVER: fd: %d\n", curr->data);
    if (fd == curr->data) {
      found = curr;
    } else {
      curr = curr->next;
    }
  }

  UNLOCK(&q->qlock);
  return found;

}


int cancella_nodo_coda(Coda *q, int fd) {

  if (q == NULL) {
    errno = EINVAL;
    return -1;
  }

  LOCK(&q->qlock);
  Nodo* temp = q->head;
  Nodo* prev = NULL;

  if (temp != NULL && temp->data == fd) {  // Se la testa conteneva fd
    q->head = temp->next;
    free((void*) temp);
    q->qlen   -= 1;
    if (q->qlen == 0) {
      if (&q->qlock)  pthread_mutex_destroy(&q->qlock);
      if (&q->qcond)  pthread_cond_destroy(&q->qcond);
      free(q);
      return 1;
    } else {
      q->qlen -= 1;
      UNLOCK(&q->qlock);
    }
    return 0;

  } else {
    while (temp != NULL && temp->data != fd) {
      prev = temp;
      temp = temp->next;
    }

  if (temp == NULL) {    // Se fd non era presente
    UNLOCK(&q->qlock);
    return 0;
  }

  prev->next = temp->next;

  free((void*)temp);

  }

  q->qlen -= 1;

  UNLOCK(&q->qlock);

  return 0;

}


unsigned long lung_coda(Coda *q) {

    LOCK(&q->qlock);
    unsigned long len = q->qlen;
    UNLOCK(&q->qlock);
    return len;
}
