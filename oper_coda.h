#ifndef OPER_CODA_H
#define OPER_CODA_H

#include <pthread.h>
#include "client_def.h"

#define MAX_NAME_LENGTH 256

// Tutte le operazioni devono essere chiamate da un solo thread


// Elemento della coda.

typedef struct Oper {
    input_opt  opt; // enum operazione
    char       arg[MAX_NAME_LENGTH]; // argomento operazione
    char       dirname[MAX_NAME_LENGTH]; // eventuale directory per opt. di read/write
    struct Oper *next;
} OperNodo_t;

// Struttura dati coda.

typedef struct OperCoda {
    OperNodo_t        *head;    // elemento di testa
    OperNodo_t        *tail;    // elemento di coda
    unsigned long  qlen;    // lunghezza
} OperCoda_t;



// Alloca ed inizializza una coda

OperCoda_t *oper_coda_init();

// Cancella una coda allocata con oper_coda_init.

void oper_coda_canc(OperCoda_t *q);

// Inserisce un nuovo nodo (operazione) nella coda

int oper_coda_ins(OperCoda_t *q, int opt, char *arg);

// Setta q->dirname =  dirname, se l'operazione contenuta nella tail della coda è WRITEDIRNAME o WRITELIST

int oper_coda_setWDirname(OperCoda_t *q, char *dirname);

// Setta q->dirname =  dirname, se l'operazione contenuta nella tail della coda è READLIST o READN

int oper_coda_setRDirname(OperCoda_t *q, char *dirname);

// Setta q->dirname =  dirname, se l'operazione contenuta nella tail della coda è APPEND

int oper_coda_setADirname(OptQueue_t *q, char *dirname);


#endif /* OPER_CODA_H */
