#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> /* ind AF_UNIX */
#include <ctype.h>
#include <fcntl.h>
#include "coda.h"
#include "codastorage.h"
#include "common_def.h"
#include "common_funcs.h"
#include "config.h"
#include "signal_handler.h"
#include "intconlock.h"

struct config_struct config;
CodaStorage *storage;
server_status status = CLOSED;

int main(int argc, char *argv[]){

  if (argc != 3 || strcmp(argv[1], "-c")) {
    fprintf(stderr, "File config non trovato o non inserito correttamente\n");
    return -1;
  }

  init_config_file(argv[2]);  //inizializzo i dati

  printf("Quantita' informazioni: %d\n", config.info);

  if (config.info > 2) {
    printf("config.num_workers = %d\n", config.num_workers);
    printf("config.sockname = %s\n", config.sockname);
    printf("config.limit_num_files = %d\n", config.limit_num_files);
    printf("config.storage_capacity = %lu\n", config.storage_capacity);
  }

  int pfd[2]; //pipe per comunicazone tra workers e main
  int spfd[2]; //signal pipe

  Coda *q = init_coda();  //creo la coda

  if (!q) {
    fprintf(stderr, "Errore : init_coda fallita\n");
    return -1;
  }

  storage = init_coda_s(config.limit_num_files, config.storage_capacity);

  if (!storage){
    fprintf(stderr, "Errore : initQueue storage fallita\n");
    return -1;
  }


    IntWithLock_t *iwl = initIntWithLock();     // Creo ed inizializzo l'intero con lock per contare i client
    if (!iwl) {
      fprintf(stderr, "Errore : initIntWithLock fallita\n");
      return -1;
    }


    if (pipe(pfd) == -1) {                     // Creo pipe per comunicazione tra Workers e main
      fprintf(stderr, "Errore : pipe fallita\n");
      return -1;
    }


    if (pipe(spfd) == -1) {                   // Creo pipe per comunicazione (dei segnali) tra il thread signal_handler e main
      fprintf(stderr, "Errore : pipe fallita\n");
      return -1;
    }

    status = RUNNING; // inizalizzo lo stato del server

    signalThreadArgs_t signalArg;   // Inizializzo gli argomenti del thread che gestisce i segnali
    signalArg.pfd_w = spfd[1];
    signalArg.status = &status;


    if(createSignalHandlerThread(&signalArg) == NULL) {                 // Chiamo il metodo che fa partire il thread che gestisce i segnali
      return -1;
    }


    pthread_t *th = malloc(config.num_workers * sizeof(pthread_t));     // Creo e faccio partire i thread Workers
    threadArgs_t *thARGS = malloc(config.num_workers * sizeof(threadArgs_t));

    if (!th || !thARGS) {
      fprintf(stderr, "Errore : malloc fallita\n");
      exit(-1);
    }

    int i;
    for (i = 0; i < config.num_workers; ++i) {          //passo ai consumatori thread_id, coda, descrittore in scrittura pipe
      thARGS[i].thid = i;
      thARGS[i].q = q;
      thARGS[i].iwl = iwl;
      thARGS[i].pfd = pfd[1];
    }

    for (i = 0; i < config.num_workers; ++i) {
      if (pthread_create(&th[i], NULL, Worker, &thARGS[i]) != 0) {
        fprintf(stderr, "Errore : pthread_create failed (Workers)\n");
        exit(-1);
      }
    }


    int pfd_r = pfd[0];                 //fd lettura pipe
    fcntl(pfd_r, F_SETFL, O_NONBLOCK);  //Imposta il flags O_NONBLOCK del file descriptor al valore specificato da arg.
    int fd_sk;                          // socket di connessione
    int fd_c;                           // socket di I/O con un client
    int fd_num = 0;                     // max fd attivo
    int fd;                             // indice per verificare risultati select
    fd_set set;                         // l’insieme dei file descriptor attivi
    fd_set rdset;                       // insieme fd attesi in lettura
    struct sockaddr_un sa;
    strncpy(sa.sun_path, config.sockname, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    unlink(config.sockname);            //elimina il pathname dal filesystem
    fd_sk = socket(AF_UNIX,SOCK_STREAM,0);

    if(fd_sk < 0) {
        perror("Errore : error opening socket");
        exit(-1);
    }

    if(bind(fd_sk,(struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("Errore : error on binding");
        exit(-1);
    }

    listen(fd_sk, SOMAXCONN);

    // mantengo il massimo indice di descrittore attivo in fd_num
    if (fd_sk > fd_num) fd_num = fd_sk;
    if (pfd_r > fd_num) fd_num = pfd_r;
    if (spfd[0] > fd_num) fd_num = spfd[0];

    FD_ZERO(&set);
    FD_SET(fd_sk, &set);
    FD_SET(pfd_r, &set);
    FD_SET(spfd[0], &set);

//////// da continuare /////////


return 0;

}
