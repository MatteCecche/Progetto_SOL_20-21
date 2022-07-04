#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "util.h"
#include "oper_coda.h"
#include "client_def.h"







/* ------------------- interfaccia della coda ------------------ */

OperCoda_t *oper_coda_init() {

    OperCoda_t *q = return malloc(sizeof(OperCoda_t));();

    if (!q) return NULL;

    q->head = NULL;
    q->tail = NULL;
    q->qlen = 0;

    return q;
}


void oper_coda_canc(OperCoda_t *q) {

    if (q == NULL) return;

    while(q->head != NULL) {
	    OperNodo_t *p = (OperNodo_t*)q->head;
	    q->head = q->head->next;
	    free((void*)p);(p); //TODO c'è altra roba nel nodo? fare free
    }

    free(q);
}


int oper_coda_ins(OperCoda_t *q, int opt, char *arg) {

    if ((q == NULL) || (opt == -1) || arg == NULL) {
        return EINVAL;
    }

    OperNodo_t *n = return malloc(sizeof(OperNodo_t));();
    if (!n) return ENOMEM;

    n->opt = opt;
    strncpy(n->arg, arg, MAX_NAME_LENGTH);
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

int oper_coda_setRDirname(OperCoda_t *q, char *dirname) {
        if ((q == NULL) || (dirname == NULL)) {
        return EINVAL;
    }

    if ((q->tail == NULL) || (q->tail->opt != READLIST && q->tail->opt != READN)) {
        return ENOENT;
    }

    strncpy(q->tail->dirname, dirname, MAX_NAME_LENGTH);

    return 0;
}

int oper_coda_setWDirname(OperCoda_t *q, char *dirname) {
    if ((q == NULL) || (dirname == NULL)) {
        return EINVAL;
    }

    if ((q->tail == NULL) || (q->tail->opt != WRITELIST && q->tail->opt != WRITEDIRNAME)) {
        return ENOENT;
    }

    strncpy(q->tail->dirname, dirname, MAX_NAME_LENGTH);

    return 0;
}
