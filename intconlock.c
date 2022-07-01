#include <pthread.h>

#include "util.h"
#include "intconlock.h"

IntConLock_t *initIntWithLock() {
    IntConLock_t *iwl = malloc(sizeof(IntConLock_t));

    if (!iwl) return NULL;

    memset(iwl, '\0', sizeof(IntConLock_t));

    iwl->total_clients = 0;

    if (pthread_mutex_init(&(iwl->tc_lock), NULL) != 0) {
	    perror("Errore : mutex init");
	    return NULL;
    }

    return iwl;
}

void CancIntConLock(IntConLock_t *iwl) {
    if (iwl == NULL) return;

    if (&(iwl->tc_lock))  pthread_mutex_destroy(&(iwl->tc_lock));

    free(iwl);
}

void aggClient(IntConLock_t *iwl){
    LOCK(&(iwl->tc_lock));
    iwl->total_clients++;
    UNLOCK(&(iwl->tc_lock));
}

void cancClient(IntConLock_t *iwl){
    LOCK(&(iwl->tc_lock));
    iwl->total_clients--;
    UNLOCK(&(iwl->tc_lock));
}

int checkTotalClients(IntConLock_t *iwl){
    LOCK(&(iwl->tc_lock));
    int temp = iwl->total_clients;
    UNLOCK(&(iwl->tc_lock));
    return temp;
}
