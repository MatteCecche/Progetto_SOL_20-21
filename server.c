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

char* myStrerror (int e);      //da fare
int requestHandler (int myid, int fd, msg_request_t req);     //da fare
int aggiorna(fd_set *set, int fd_num);
void *Worker(void *arg);

struct config_struct config;
CodaStorage *storage;
server_status status = CLOSED;

int main(int argc, char *argv[]){

  if (argc != 3 || strcmp(argv[1], "-c")) {
    fprintf(stderr, "File config non trovato o non inserito correttamente\n");
    return -1;
  }

  init_config_file(argv[2]);        //inizializzo i dati

  printf("Quantita' informazioni: %d\n", config.info);

  if (config.info > 2) {
    printf("config.num_workers = %d\n", config.num_workers);
    printf("config.sockname = %s\n", config.sockname);
    printf("config.limit_num_files = %d\n", config.limit_num_files);
    printf("config.storage_capacity = %lu\n", config.storage_capacity);
  }

  int pfd[2];                       //pipe per comunicazone tra workers e main
  int spfd[2];                      //signal pipe

  Coda *q = init_coda();            //creo la coda

  if (!q) {
    fprintf(stderr, "Errore : init_coda fallita\n");
    return -1;
  }

  storage = init_coda_s(config.limit_num_files, config.storage_capacity);

  if (!storage){
    fprintf(stderr, "Errore : initQueue storage fallita\n");
    return -1;
  }


    IntConLock_t *iwl = initconlock();     // Creo ed inizializzo l'intero con lock per contare i client
    if (!iwl) {
      fprintf(stderr, "Errore : initconlock fallita\n");
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

    int temp = -1;

    while (((temp = checkTotalClients(iwl)) != 0 && status == CLOSING) || status == RUNNING) {
      if (config.v > 2) printf("SERVER : totalCLients = %d\n", temp);

      rdset = set;                                          // preparo maschera per select

      if (select(fd_num+1, &rdset, NULL, NULL, NULL) == -1) {
        fprintf(stderr, "Errore : select fallita\n");
        exit(-1);
      } else {                                              // select OK

        if (config.v > 2) printf("SERVER : select ok\n");
          fflush(stdout);

          for (fd = 0; fd <= fd_num; fd++) {

            if (FD_ISSET(fd, &rdset)) {

              if (fd == fd_sk) {                            // sock connect pronto
                fd_c = accept(fd,NULL,0);

                if (fd_c == -1) {
                  fprintf(stderr, "Errore accept\n");
                  exit(-1);
                }

                if (config.v > 2) printf("SERVER : Appena fatta accept, nuovo fd:%d\n", fd_c);
                  fflush(stdout);

                  FD_SET(fd_c, &set);
                  addClient(iwl);
                  if (fd_c > fd_num) fd_num = fd_c;
                  } else if (fd == pfd_r) {                   //aggiungo in set gli fd (se > 0) che leggo dalla pipe (con workers)

                    int num;
                    int nread;

                    while ((nread = readn(fd, &num, sizeof(num))) == sizeof(num)) {

                      if (config.v > 2) printf("SERVER : Letto fd:%d dalla pipe\n", num);
                        fflush(stdout);

                        if (num > 0) {
                          FD_SET(num, &set);

                          if (num > fd_num) fd_num = num;
                        }
                      }

                      if (nread > 0) {
                        fprintf(stderr, "Errore nread %d main, lettura parziale\n", nread);
                        fflush(stderr);
                      }

                  } else if (fd == spfd[0]) {                 //pipe con il signal_handler
                    FD_CLR(spfd[0], &set);                    //tolgo pipe con signal_handler dal set
                    fd_num = aggiorna(&set, fd_num);
                    close(spfd[0]);

                    if (status == CLOSING) {

                      if (config.v > 1) printf("SERVER : Non accetto nuove connessioni\n");
                        FD_CLR(fd_sk, &set);                  //tolgo socket di connessione dal set
                        fd_num = aggiorna(&set, fd_num);
                        close(fd_sk);
                    } else if (status == CLOSED) {

                      if (config.v > 1) printf("SERVER : Non accetto nuove richieste\n");  // (finisco il ciclo for in cui sono, quando torno al while esco)
                      } else {
                        fprintf(stderr, "SERVER : Stato server non consistente\n");

                        status = CLOSED;                        //come quello sopra (finisci di servire le richieste e chiudi)
                      }
                    } else {                                    // sock I/0 pronto

                      if (ins_coda(q, fd) == -1) {
                      fprintf(stderr, "Errore : push\n");
                      exit(-1);
                      }
                    FD_CLR(fd, &set);                           // tolgo fd dal set
                    fd_num = aggiorna(&set, fd_num);

                    if (config.v > 2) printf("SERVER : Master pushed <%d>\n", fd);
                      fflush(stdout);
                   }
                }
              }
           }
       }


return 0;

}





int aggiorna(fd_set *set, int fd_num) {

  int max_fd = 0;
  int i;

  for (i=0; i <= fd_num; i++) {

    if (FD_ISSET(i, set)) {

      max_fd = i;

    }
  }

  return max_fd;

}


void *Worker(void *arg) {

  CodaStorage *q = ((threadArgs_t *)arg)->q;
  intconlockLock_t *iwl = ((threadArgs_t *)arg)->iwl;
  int myid = ((threadArgs_t *)arg)->thid;
  int pfd_w = ((threadArgs_t *)arg)->pfd; //fd lettura pipe

  int nread; // numero caratteri letti
  msg_request_t req; // messaggio richiesta dal client

  size_t consumed = 0;
  while (1) {

    int fd;
    fd = estrai_coda(q);

    if (fd == -1) {

      if (config.v > 1) printf("SERVER : Worker %d, ha consumato  <%ld> messaggi, ora esiste\n", myid, consumed);
        fflush(stdout);
        return NULL;
    }
    ++consumed;

    if (config.v > 2) printf("SERVER : Worker %d: estratto <%d>\n", myid, fd);
    fflush(stdout);

    nread = readn(fd, &req, sizeof(msg_request_t));

    if (nread == 0) {/* EOF client finito */

      if (config.v > 2) printf("SERVER : Worker %d, chiudo:%d\n", myid, fd);
      fflush(stdout);

      int esito = removeFile_coda_s(storage_q, fd);

      if (config.v > 1) printf("SERVER : Close fd: %d, esito: %s\n", fd, myStrerror(esito));

      close(fd);
      fd = -fd;

      deleteClient(iwl);

      } else if (nread != sizeof(msg_request_t)) {
        fprintf(stderr, "Errore : nread worker, lettura parziale\n");
        fflush(stderr);

        if (config.v > 2) printf("SERVER : Worker %d, chiudo:%d\n", myid, fd);

        int esito = removeFile_coda_s(storage_q, fd);

        if (config.v > 1) printf("SERVER : Close fd: %d, esito: %s\n", fd, myStrerror(esito));
        fflush(stdout);

        close(fd);
        fd = -fd;

        deleteClient(iwl);

      } else { /* nread == sizeof(msg_request_t) */

        if (config.v > 1) printf("SERVER : Worker %d, Server ha preso : %d, da  %d\n", myid, req.op, fd);
        fflush(stdout);

        if (requestHandler(myid, fd, req) != 0) { // client disconnesso
          fd = -fd; // in modo che nel main non si riaggiunga nel set

          deleteClient(iwl);

        }
      }

        // scrittura di fd nella pipe
      if (writen(pfd_w, &fd, sizeof(fd)) != sizeof(fd)) {
        fprintf(stderr, "Errore : writen worker, parziale\n");
        fflush(stderr);
        exit(-1);
      } else {

        if (config.v > 2) printf("SERVER : fd %d messo nella pipe\n", fd);
        fflush(stdout);
      }

      memset(&req, '\0', sizeof(req));

    }

    if (config.v > 1) printf("SERVER : Worker %d, ha consumato <%ld> messaggi, adesso esce\n", myid, consumed);
    fflush(stdout);


    return NULL;


}




//da fare requestHandler
