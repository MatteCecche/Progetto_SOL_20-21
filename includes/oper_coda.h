#define _GNU_SOURCE
#define PATH_MAX        4096
#ifndef OPER_CODA_H
#define OPER_CODA_H

#include <pthread.h>
#include <limits.h>
#include "def_client.h"
#include "def_tipodato.h"



// ------------------------------------------------------ Elemento della coda delle operazioni ------------------------------------------------------ //

typedef struct Opt {

    input_opt     opt;                          // enum operazione
    char          f[PATH_MAX];                  // flag open
    char          arg[PATH_MAX];                // argomento operazione
    char          dirname[PATH_MAX];            // eventuale directory per opt. di read/write/append
    struct Opt    *next;

} OperNodo_t;


// ------------------------------------------------------ Struttura dati coda delle operazioni ----------------------------------------------------- //

typedef struct OperCoda {

    OperNodo_t        *head;    // elemento di testa
    OperNodo_t        *tail;    // elemento di coda
    unsigned long      qlen;    // lunghezza

} OperCoda_t;



// ---------------------------------------- Tutte le operazioni devono essere chiamate da un solo thread ------------------------------------------ //


// ------------------------------ //
// Alloca ed inizializza una coda //
// ------------------------------ //

OperCoda_t *init_coda_oper();


// --------------------------------------------- //
// Cancella una coda allocata con init_coda_oper //
// --------------------------------------------- //

void canc_coda_oper(OperCoda_t *q);


// ----------------------------------------------- //
// Inserisce un nuovo nodo (operazione) nella coda //
// ----------------------------------------------- //

void ins_coda_oper(OperCoda_t *q, int opt, char *arg);


// ------------------------------------------------------------------------------------------------------- //
// Setta q->dirname =  dirname, se l'operazione contenuta nella tail della coda è WRITEDIRNAME o WRITELIST //
// ------------------------------------------------------------------------------------------------------- //

void setWDirname_coda_oper(OperCoda_t *q, char *dirname);


// ----------------------------------------------------------------------------------------------- //
// Setta q->dirname =  dirname, se l'operazione contenuta nella tail della coda è READLIST o READN //
// ----------------------------------------------------------------------------------------------- //

void setRDirname_coda_oper(OperCoda_t *q, char *dirname);


// ------------------------------------------------------------------------------------- //
// Setta q->dirname =  dirname, se l'operazione contenuta nella tail della coda è APPEND //
// ------------------------------------------------------------------------------------- //

void setADirname_coda_oper(OperCoda_t *q, char *dirname);



#endif
