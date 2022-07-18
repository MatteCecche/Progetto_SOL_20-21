#ifndef CODA_H
#define CODA_H

#include <pthread.h>

// ----------------------------------- Elemento della coda ----------------------------------- //

typedef struct Node {

    int           data;
    struct Node * next;

} Nodo_t;

// ----------------------------------- Struttura dati coda ----------------------------------- //

typedef struct coda {

    Nodo_t            *head;    // elemento di testa
    Nodo_t            *tail;    // elemento di coda
    unsigned long     qlen;     // lunghezza
    pthread_mutex_t   qlock;
    pthread_cond_t    qcond;

} Coda_t;


// ---------------------------------------------------------------- //
//  Alloca ed inizializza una coda. Deve essere chiamata da un solo //
//  thread (tipicamente il thread main).                            //
// ---------------------------------------------------------------- //

Coda_t *init_coda(FILE *l, pthread_mutex_t ml);


// ------------------------------------------------------------------ //
//  Cancella una coda allocata con init_coda. Deve essere chiamata da //
//  da un solo thread (tipicamente il thread main).                   //
// ------------------------------------------------------------------ //

void canc_coda(Coda_t *q);


// ------------------------------ //
// Inserisce un dato nella coda.  //
// ------------------------------ //

int ins_coda(Coda_t *q, int data);


// -------------------------- //
// Estrae un dato dalla coda. //
// -------------------------- //

int estrai_coda(Coda_t *q);


// ------------------- //
// Cerca fd nella coda //
// ------------------- //

Nodo_t* trova_coda(Coda_t *q, int fd);


// -------------------------------------------------------------------------------------- //
// Rimuove (se c'è) fd dalla coda (la prima occorrenza (ce ne dovrebbe essere solo una))  //
// -------------------------------------------------------------------------------------- //

int cancNodo_coda(Coda_t *q, int fd);


// ---------------------------------------------------------------- //
// Ritorna la lunghezza attuale della coda passata come parametro.  //
// ---------------------------------------------------------------- //

unsigned long lung_coda(Coda_t *q);




#endif
