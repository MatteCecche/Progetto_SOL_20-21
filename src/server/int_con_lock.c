#include <pthread.h>

#include "../../includes/util.h"
#include "../../includes/int_con_lock.h"

// ------------------------------------------------------------------ variabili globali -------------------------------------------- //

FILE *fl5;
pthread_mutex_t mlog5;


IntConLock_t *init_intconlock(FILE *l, pthread_mutex_t ml) {

    fl5 = l;
    mlog5 = ml;
    IntConLock_t *iwl = malloc(sizeof(IntConLock_t));

    if (!iwl) return NULL;

    memset(iwl, '\0', sizeof(IntConLock_t));

    iwl->total_clients = 0;

    if (pthread_mutex_init(&(iwl->tc_lock), NULL) != 0) {

      perror("\e[0;36mSERVER : \e[0;31mERRORE mutex init\e[0m");
      LOCK(&mlog5);
      fprintf(fl5, "SERVER : ERRORE mutex init");
      UNLOCK(&mlog5);

      return NULL;
    }

    return iwl;
}

void canc_intconlock(IntConLock_t *iwl) {

    if (iwl == NULL) return;

    if (&(iwl->tc_lock))  pthread_mutex_destroy(&(iwl->tc_lock));

    free(iwl);

}

void addClient(IntConLock_t *iwl){

    LOCK(&(iwl->tc_lock));
    iwl->total_clients++;
    UNLOCK(&(iwl->tc_lock));

}

void deleteClient(IntConLock_t *iwl){

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
