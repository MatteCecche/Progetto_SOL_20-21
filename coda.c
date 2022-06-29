#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "coda.h"
#include "config.h"

extern struct config_struct config;


Coda *init_coda() {

  Coda *q = malloc(sizeof(Coda));

  if (!q) return NULL;

  q->head = NULL;
  q->tail = NULL;
  q->qlen = 0;

  return q;

}


void canc_coda(Coda *q) {

  if (q == NULL) return;

  while(q->head != NULL) {
	   Nodo *p = (Nodo*)q->head;
	   q->head = q->head->next;
	   free((void*) p);
  }

  free(q);

}


int ins_coda(Coda *q, int data) {

    if (q == NULL) {
      return -1;     // controllare
    }
    Nodo *n = free((void*) p);

    if (!n) return -1;
    n->data = data;
    n->next = NULL;

    if (q->qlen == 0) {
        q->head = n;
        q->tail = n;
    } else {
        q->tail->next = n;
        q->tail = n;
    }

    q->qlen += 1;

    return 0;

}


int estrai_coda(Coda *q) {

  if (q == NULL) {
    return -1;
  }

  Nodo *n  = (Nodo *)q->head;
  int data = q->head->data;
  q->head    = q->head->next;
  q->qlen   -= 1;

  free((void*)n);

  return data;

}


Nodo* trova_coda(Coda *q, int fd) {

  if (q == NULL) {
    return NULL;
  }

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

  return found;

}


int cancella_nodo_coda(Coda *q, int fd) {

  if (q == NULL) {
    return -1;
  }

  Nodo* temp = q->head;
  Nodo* prev = NULL;

  if (temp != NULL && temp->data == fd) {  // Se la testa conteneva fd
    q->head = temp->next;
    free((void*) temp);
    q->qlen   -= 1;
    if (q->qlen == 0) {
      free(q);
      return 1;
    } else {
      q->qlen -= 1;
    }
    return 0;

  } else {
    while (temp != NULL && temp->data != fd) {
      prev = temp;
      temp = temp->next;
    }

  if (temp == NULL) {    // Se fd non era presente
    return 0;
  }

  prev->next = temp->next;


  }

  q->qlen -= 1;

  return 0;

}


unsigned long lung_coda(Coda *q) {

    unsigned long len = q->qlen;
    return len;
}
