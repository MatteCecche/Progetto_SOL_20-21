#ifndef INT_WITH_LOCK_H
#define INT_WITH_LOCK_H

#include <pthread.h>

// ------------------------------------------------------ intero con lock ------------------------------------------------------ //

typedef struct IntConLock {

    int total_clients;            // numero dei client connessi
    pthread_mutex_t tc_lock;

} IntConLock_t;


// ---------------------------------------------------------------------------------------------- //
// Inizializza la struttura, settando 'total_clients' a zero, ed inizializzando il mutex tc_lock  //
// Deve essere chiamata da un solo thread (tipicamente il thread main).                           //
// ---------------------------------------------------------------------------------------------- //

IntConLock_t *init_intconlock(FILE *l, pthread_mutex_t ml);


// -------------------------------------------------------------------- //
// Cancella la struttura allocata con init_IntConLock.                  //
// Deve essere chiamata da un solo thread (tipicamente il thread main)  //
// -------------------------------------------------------------------- //

void canc_intconlock(IntConLock_t *iwl);


// -------------------------------------------- //
// Aumenta di uno l'intero (iwl->total_clients) //
// -------------------------------------------  //

void addClient(IntConLock_t *iwl);


// ------------------------------------------------ //
// Diminuisce di uno l'intero (iwl->total_clients)  //
// -----------------------------------------------  //

void deleteClient(IntConLock_t *iwl);


// ------------------------------------------ //
// Restituisce l'intero (iwl->total_clients)  //
// ------------------------------------------ //

int checkTotalClients(IntConLock_t *iwl);



#endif
