#ifndef INT_WITH_LOCK_H
#define INT_WITH_LOCK_H

#include <pthread.h>


typedef struct IntWithLock {      // intero con lock

    int total_clients;            // numero dei client connessi
    pthread_mutex_t tc_lock;

} IntConLock_t;

// Inizializza la struttura, settando 'total_clients' a zero, ed inizializzando il mutex tc_lock
IntConLock_t *initIntWithLock();

// Cancella la struttura allocata con initIntWithLock.
void CancIntConLock(IntConLock_t *iwl);

// Aumenta di uno l'intero (iwl->total_clients)
void aggClient(IntConLock_t *iwl);

// Diminuisce di uno l'intero (iwl->total_clients)
void cancClient(IntConLock_t *iwl);

// Restituisce l'intero (iwl->total_clients)
int checkTotalClients(IntConLock_t *iwl);


#endif
