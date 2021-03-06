#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "../../includes/util.h"
#include "../../includes/oper_coda.h"
#include "../../includes/def_client.h"
#include "../../includes/def_tipodato.h"




// ----------------------------------------- funzioni di utilita' ------------------------------------------ //

static inline OperNodo_t  *allocNodo(){

  return malloc(sizeof(OperNodo_t));

}

static inline OperCoda_t *allocCoda(){

  return malloc(sizeof(OperCoda_t));

}

static inline void freeNode(OperNodo_t *node){

  free((void*)node);

}


// ----------------------------------------- interfaccia della coda ---------------------------------------- //

OperCoda_t *init_coda_oper() {

    OperCoda_t *q = allocCoda();

    if (!q) return NULL;

    q->head = NULL;
    q->tail = NULL;
    q->qlen = 0;

    return q;
}


void canc_coda_oper(OperCoda_t *q) {

    if (q == NULL) return;

    while(q->head != NULL) {

      OperNodo_t *p = (OperNodo_t*)q->head;
	    q->head = q->head->next;
	    freeNode(p);
    }

    free(q);
}


void ins_coda_oper(OperCoda_t *q, int opt, char *arg) {

    if ((q == NULL) || (opt == -1) || arg == NULL) {

        return EINVAL;
    }

    OperNodo_t *n = allocNodo();
    if (!n) return ENOMEM;
    memset(n, '\0', sizeof(OperNodo_t));


    n->opt = opt;
    strncpy(n->arg, arg, PATH_MAX);
    if (strchr(arg,'@') == NULL ) strncpy(n->f, "O_CREATE_LOCK", PATH_MAX);       // se non c'e' il flag default = O_CREATE_LOCK
    else {
      char *token;
      strtok_r(n->arg, "@", &token);
      strncpy(n->f, token, PATH_MAX);
    }
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

void setRDirname_coda_oper(OperCoda_t *q, char *dirname) {

    if ((q == NULL) || (dirname == NULL)) {

        return EINVAL;
    }

    if ((q->tail == NULL) || (q->tail->opt != READLIST && q->tail->opt != READN)) {

        return ENOENT;
    }

    strncpy(q->tail->dirname, dirname, PATH_MAX);

    return 0;
}

void setWDirname_coda_oper(OperCoda_t *q, char *dirname) {

    if ((q == NULL) || (dirname == NULL)) {

        return EINVAL;
    }

    if ((q->tail == NULL) || (q->tail->opt != WRITELIST && q->tail->opt != WRITEDIRNAME)) {

        return ENOENT;
    }

    strncpy(q->tail->dirname, dirname, PATH_MAX);

    return 0;
}

void setADirname_coda_oper(OperCoda_t *q, char *dirname) {

    if ((q == NULL) || (dirname == NULL)) {

        return EINVAL;
    }

    if ((q->tail == NULL) || (q->tail->opt != APPEND)) {

        return ENOENT;
    }

    strncpy(q->tail->dirname, dirname, PATH_MAX);

    return 0;
}
