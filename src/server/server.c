#define _GNU_SOURCE
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
#include <sys/un.h>
#include <ctype.h>
#include <fcntl.h>


#include "../../includes/coda.h"
#include "../../includes/coda_storage.h"
#include "../../includes/def_tipodato.h"
#include "../../includes/fun_descrit.h"
#include "../../includes/config.h"
#include "../../includes/signal_handler.h"
#include "../../includes/int_con_lock.h"
#include "../../includes/util.h"



// --------------------------------- tipo di dato usato per passare gli argomenti ai thread worker --------------------------------- //

typedef struct threadArgs {

    int pfd;                    // fd lettura pipe
    int thid;                   // id del thread worker
    Coda_t *q;
    IntConLock_t *iwl;          // numero client attivi

} threadArgs_t;



// ------------------------------------------------------------------ variabili globali -------------------------------------------- //

struct config_struct config;          // variabile che memorizza i dati della configurazione con cui è stato avviato il server

CodaStorage_t *storage_q;             // puntatore alla coda dei file dello storage

server_status status = CLOSED;        // variabile che rappresenta lo stato del server

FILE* fl;

pthread_mutex_t mlog = PTHREAD_MUTEX_INITIALIZER;




char* myStrerror (int e);


// -------------------------------------------------------------- //
//  Gestisce la richiesta del client, interagendo con lo storage, //
//  ed invia il messaggio di risposta al client                   //
// -------------------------------------------------------------- //

int requestHandler (int myid, int fd, msg_richiesta_t req);


// ---------------------- //
// ritorna max fd attivo  //
// ---------------------- //

int aggiorna(fd_set *set, int fd_num);


// ------------------------------------ //
// funzione eseguita dal thread worker  //
// ------------------------------------ //

void *Worker(void *arg);


int main(int argc, char *argv[]){

    int pfd[2];                 //pipe per comunicazione tra workers e main
    int spfd[2];                //pipe per comunicazione (dei segnali) tra il thread signal_handler e main


    if (argc != 3 || strcmp(argv[1], "-c")) {

        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE Usage -c config_file\n\e[0m");

        return -1;

    }
    read_config_file(argv[2]);

    printf("%s\n",config.path_filelog);

    if ((fl=fopen(config.path_filelog, "w+t"))==NULL)
      fprintf(stderr, "Errore nell'apertura del filelog\n");

    Coda_t *q = init_coda(fl, mlog);                // Creo ed inizializzo la coda utilizzata dal main per comunicare i fd agli Workers
    if (!q){

        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE init_coda fallita\n\e[0m");
        LOCK(&mlog);
        fprintf(fl, "SERVER : ERRORE init_coda fallita\n");
        UNLOCK(&mlog);

        return -1;
    }

    storage_q = init_coda_stor(config.limit_num_files, config.storage_capacity, fl, mlog);      // Creo ed inizializzo la coda dello storage

    if (!storage_q){

        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE init_coda_stor fallita\n\e[0m");
        LOCK(&mlog);
        fprintf(fl, "SERVER : ERRORE init_coda_stor fallita\n");
        UNLOCK(&mlog);

        return -1;
    }

    IntConLock_t *iwl = init_intconlock(fl, mlog);                    // Creo ed inizializzo l'intero con lock per contare il numero di client
    if (!iwl){

        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE init_intconlock fallita\n\e[0m");
        LOCK(&mlog);
        fprintf(fl, "SERVER : ERRORE init_intconlock fallita\n");
        UNLOCK(&mlog);

        return -1;
    }

    if (pipe(pfd) == -1) {                                    // Creo pipe per comunicazione tra Workers e main

        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE pipe fallita\n\e[0m");
        LOCK(&mlog);
        fprintf(fl, "SERVER : ERRORE pipe fallita\n");
        UNLOCK(&mlog);

        return -1;
    }

    if (pipe(spfd) == -1) {                                   // Creo pipe per comunicazione (dei segnali) tra il thread signal_handler e main

        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE pipe fallita\n\e[0m");
        LOCK(&mlog);
        fprintf(fl, "SERVER : ERRORE pipe fallita\n");
        UNLOCK(&mlog);

        return -1;
    }


    status = RUNNING;                                         // inizalizzo lo stato del server
    signalThreadArgs_t signalArg;                             // Inizializzo gli argomenti del thread che gestisce i segnali
    signalArg.pfd_w = spfd[1];
    signalArg.status = &status;

    if(createSignalHandlerThread(&signalArg, fl, mlog) == NULL) {       // Chiamo il metodo che fa partire il thread che gestisce i segnali

        return -1;

    }


    pthread_t *th = malloc(config.num_workers * sizeof(pthread_t));                 // Creo e faccio partire i thread Workers (con gli opportuni argomenti)
    threadArgs_t *thARGS = malloc(config.num_workers * sizeof(threadArgs_t));

    if (!th || !thARGS){

        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE malloc fallita\n\e[0m");
        LOCK(&mlog);
        fprintf(fl, "SERVER : ERRORE malloc fallita\n");
        UNLOCK(&mlog);
        exit(-1);
    }

    int i;
    for (i = 0; i < config.num_workers; ++i){                                     //passo ai consumatori thread_id, coda, descrittore in scrittura pipe

        thARGS[i].thid = i;
        thARGS[i].q = q;
        thARGS[i].iwl = iwl;
        thARGS[i].pfd = pfd[1];
    }

    for (i = 0; i < config.num_workers; ++i){

        if (pthread_create(&th[i], NULL, Worker, &thARGS[i]) != 0){

            fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE pthread_create fallita (Workers)\n\e[0m");
            LOCK(&mlog);
            fprintf(fl, "SERVER : ERRORE pthread_create fallita (Workers)\n");
            UNLOCK(&mlog);
            exit(-1);
        }
    }


    int pfd_r = pfd[0];                                                           //fd lettura pipe
    fcntl(pfd_r, F_SETFL, O_NONBLOCK);                                            //esegue un operazione su pfd_r determinata da F_SETFL(Imposta i flags del file descriptor al valore specificato(in questo caso O_NONBLOCK (impedisce all'open di bloccare per molto tempo l'apertura del file)))

    int fd_sk;                                                                    // socket di connessione
    int fd_c;                                                                     // socket di I/O con un client
    int fd_num = 0;                                                               // max fd attivo
    int fd;                                                                       // indice per verificare risultati select
    fd_set set;                                                                   // l’insieme dei file descriptor attivi
    fd_set rdset;                                                                 // insieme fd attesi in lettura
    struct sockaddr_un sa;
    strncpy(sa.sun_path, config.sockname, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    unlink(config.sockname);                                                      // elimina il pathname dal filesystem
    fd_sk = socket(AF_UNIX,SOCK_STREAM,0);
    if(fd_sk < 0) {

        perror("\e[0;36mSERVER : \e[0;31mERRORE apertura socket\e[0m");
        LOCK(&mlog);
        fprintf(fl, "SERVER : ERRORE apertura socket");
        UNLOCK(&mlog);
        exit(-1);
    }
    if(bind(fd_sk,(struct sockaddr *)&sa, sizeof(sa)) < 0) {

        perror("\e[0;36mSERVER : \e[0;31mERRORE sul binding\e[0m");
        LOCK(&mlog);
        fprintf(fl, "SERVER : ERRORE sul binding");
        UNLOCK(&mlog);
        exit(-1);
    }
    listen(fd_sk, SOMAXCONN);

    if (fd_sk > fd_num) fd_num = fd_sk;                                           // mantengo il massimo indice di descrittore attivo in fd_num
    if (pfd_r > fd_num) fd_num = pfd_r;
    if (spfd[0] > fd_num) fd_num = spfd[0];

    FD_ZERO(&set);
    FD_SET(fd_sk, &set);
    FD_SET(pfd_r, &set);
    FD_SET(spfd[0], &set);

    int temp = -1;

    while (((temp = checkTotalClients(iwl)) != 0 && status == CLOSING) || status == RUNNING) {

        rdset = set;                                                              // preparo maschera per select

        if (select(fd_num+1, &rdset, NULL, NULL, NULL) == -1) {

            fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE select fallita\n\e[0m");
            LOCK(&mlog);
            fprintf(fl, "SERVER : ERRORE select fallita\n");
            UNLOCK(&mlog);
            exit(-1);
        } else {                                                                    // select OK

            for (fd = 0; fd <= fd_num; fd++) {

                if (FD_ISSET(fd, &rdset)) {

                    if (fd == fd_sk) {                                            // sock connect pronto

                        fd_c = accept(fd,NULL,0);
                        if (fd_c == -1) {

                            fprintf(stderr, "\e[0;36mSERVER: \e[0;31mERRORE accept\n\e[0m");
                            LOCK(&mlog);
                            fprintf(fl, "SERVER: ERRORE accept\n");
                            UNLOCK(&mlog);
                            exit(-1);
                        }

                        printf("\e[0;36mSERVER : Nuovo client connesso, nuovo fd :%d\n\e[0m", fd_c);
                        LOCK(&mlog);
                        fprintf(fl, "SERVER : Nuovo client connesso, nuovo fd:%d\n", fd_c);
                        UNLOCK(&mlog);
                        fflush(stdout);

                        FD_SET(fd_c, &set);
                        addClient(iwl);

                        if (fd_c > fd_num) fd_num = fd_c;
                    } else if (fd == pfd_r) {                                     //aggiungo in set gli fd (se > 0) che leggo dalla pipe (con workers)

                        int num;
                        int nread;

                        while ((nread = readn(fd, &num, sizeof(num))) == sizeof(num)) {

                            fflush(stdout);
                            if (num > 0) {
                                FD_SET(num, &set);
                                if (num > fd_num) fd_num = num;
                            }
                        }
                        if (nread > 0) {

                            fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE nread %d main, lettura parziale\n\e[0m", nread);
                            LOCK(&mlog);
                            fprintf(fl, "SERVER : ERRORE nread %d main, lettura parziale\n", nread);
                            UNLOCK(&mlog);
                            fflush(stderr);
                        }

                    } else if (fd == spfd[0]) {                                   // pipe con il signal_handler
                        FD_CLR(spfd[0], &set);                                    // tolgo pipe con signal_handler dal set
                        fd_num = aggiorna(&set, fd_num);
                        close(spfd[0]);

                        if (status == CLOSING) {

                              printf("\e[0;36mSERVER : Non accetto nuove connessioni\n\e[0m");
                              LOCK(&mlog);
                              fprintf(fl, "SERVER : Non accetto nuove connessioni\n");
                              UNLOCK(&mlog);

                            FD_CLR(fd_sk, &set);                                  //tolgo socket di connessione dal set
                            fd_num = aggiorna(&set, fd_num);
                            close(fd_sk);

                        }
                        else if (status == CLOSED) {

                              printf("\e[0;36mSERVER : Non accetto nuove richieste\n\e[0m");
                              LOCK(&mlog);
                              fprintf(fl, "SERVER : Non accetto nuove richieste\n");
                              UNLOCK(&mlog);

                        } else {                                                                // (finisco il ciclo for in cui sono, quando torno al while esco)
                            fprintf(stderr, "\e[0;36mSERVER : Stato server non consistente\n\e[0m");
                            LOCK(&mlog);
                            fprintf(fl, "SERVER : Stato server non consistente\n");
                            UNLOCK(&mlog);
                            status = CLOSED;
                                                                                                //come quello sopra (finisci di servire le richieste e chiudi)
                        }
                    } else {

                        if (ins_coda(q, fd) == -1) {

                            fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE : ins\n\e[0m");
                            LOCK(&mlog);
                            fprintf(fl, "SERVER : ERRORE : ins\n");
                            UNLOCK(&mlog);
                            exit(-1);
                        }

                        FD_CLR(fd, &set);                                                   // tolgo fd dal set
                        fd_num = aggiorna(&set, fd_num);

                          fflush(stdout);
                    }
                }
            }
        }
    }

    broadcast_coda_stor(storage_q);                           // svegliamo gli worker fermi su una wait (di coda_storage)

    if (CLOSED) close(fd_sk);
    unlink(config.sockname);
    close(pfd_r);
    close(spfd[0]);

    for (i = 0; i < config.num_workers; ++i){                 // quindi termino tutti i worker

        int eos = -1;
        if (ins_coda(q, eos) == -1) {

            fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE: ins\n\e[0m");
            LOCK(&mlog);
            fprintf(fl, "SERVER : ERRORE: ins\n");
            UNLOCK(&mlog);
            exit(-1);
        }
    }

    for (i = 0; i < config.num_workers; ++i)                  // aspetto la terminazione di tutti worker
        pthread_join(th[i], NULL);


    printf("\n\e[0;33m -------- Sunto operazioni effettuate --------\n");                                 // stampa del sunto delle operazioni effettuate
    printf("SERVER: Numero di file massimo memorizzato nel server: \e[0m%d\n\e[0;33m", storage_q->max_num_files);
    printf("SERVER: Dimensione massima in bytes raggiunta: \e[0m%lu\n\e[0;33m", storage_q->max_used_storage);
    printf("SERVER: Algoritmo della cache eseguito \e[0m%d volte\n\e[0;33m", storage_q->replace_occur);
    printf("SERVER: Lista dei file contenuti nello storage al momento della chiusura:\n");
    printListFiles_coda_stor(storage_q);
    printf("\e ----------------------------------------------\n\n");


    LOCK(&mlog);
    fprintf(fl, "SERVER: Dimensione massima in bytes raggiunta: %lu\n", storage_q->max_used_storage);
    fprintf(fl, "SERVER: Numero di file massimo memorizzato nel server: %d\n", storage_q->max_num_files);
    fprintf(fl, "SERVER: Algoritmo della cache eseguito %d volte\n", storage_q->replace_occur);
    UNLOCK(&mlog);

    canc_coda(q);                                               // libero memoria
    canc_coda_stor(storage_q);
    canc_intconlock(iwl);
    free(th);
    free(thARGS);
    fclose(fl);

    return 0;


}


char* myStrerror (int e) {

    if (e == -1) {

        return "Errore nella comunicazione con il client";

    } else if (e == 0) {

        return "Positivo";
    } else return strerror(e);

}


int requestHandler (int myid, int fd, msg_richiesta_t req) {

    msg_risposta_t res;
    res.result = 0;
    res.datalen = 0;                    //superfluo

    switch(req.op) {
                                        // res.result ==
                                        //                • -1: si sono verificati dei problemi sul socket fd ed è stato chiuso
                                        //                • 0: (ho già comunicato al client fd)
                                        //                • >0: errore da comunicare

        case OPENFILE: {
          if (req.flag == O_CREATE) {
            res.result = updateOpeners_coda_stor(storage_q, req.pathname, false, fd, fl, mlog);
            res.result = ins_coda_stor(storage_q, req.pathname, true, fd, fl, mlog);
            printf("\e[0;36mSERVER : l'operazione >Openfile_O_CREATE< di %s, fd: %d, e' stata eseguita con esito: %s\n\e[0m", req.pathname, fd, myStrerror(res.result));
            LOCK(&mlog);
            fprintf(fl, "SERVER : l'operazione >Openfile_O_CREATE< di %s, fd: %d, e' stata eseguita con esito: %s\n", req.pathname, fd, myStrerror(res.result));
            UNLOCK(&mlog);
            if (res.result <= 0) return res.result;

          } else if (req.flag == O_CREATE_LOCK) {

            res.result = ins_coda_stor(storage_q, req.pathname, true, fd, fl, mlog);
            printf("\e[0;36mSERVER : l'operazione >Openfile_O_CREATE_LOCK< di %s, fd: %d, e' stata eseguita con esito: %s\n\e[0m", req.pathname, fd, myStrerror(res.result));
            LOCK(&mlog);
            fprintf(fl, "SERVER : l'operazione >Openfile_O_CREATE_LOCK< di %s, fd: %d, e' stata eseguita con esito: %s\n", req.pathname, fd, myStrerror(res.result));
            UNLOCK(&mlog);
            if (res.result <= 0) return res.result;

          } else if (req.flag == O_LOCK) {

            res.result = updateOpeners_coda_stor(storage_q, req.pathname, true, fd, fl, mlog);
            printf("\e[0;36mSERVER : l'operazione >Openfile_O_LOCK< di %s, fd: %d, e'stata eseguita con esito: %s\n\e[0m", req.pathname, fd, myStrerror(res.result));
            LOCK(&mlog);
            fprintf(fl, "SERVER : l'operazione >Openfile_O_LOCK< di %s, fd: %d, e'stata eseguita con esito: %s\n", req.pathname, fd, myStrerror(res.result));
            UNLOCK(&mlog);
            if (res.result <= 0) return res.result;

          } else if (req.flag == O_NULL) {

            res.result = updateOpeners_coda_stor(storage_q, req.pathname, false, fd, fl, mlog);
            printf("\e[0;36mSERVER : l'operazione >Openfile_O_NULL< di %s, fd: %d, e' stata eseguita con esito: %s\n\e[0m", req.pathname, fd, myStrerror(res.result));
            LOCK(&mlog);
            fprintf(fl, "SERVER : l'operazione >Openfile_O_NULL< di %s, fd: %d, e' stata eseguita con esito: %s\n", req.pathname, fd, myStrerror(res.result));
            UNLOCK(&mlog);
            if (res.result <= 0) return res.result;

          } else {                      // flag non riconosciuto

            printf("\e[0;36mSERVER : l'operazione >Openfile< fd: %d, flag non riconosciuto %s\n\e[0m", fd, req.flag);
            LOCK(&mlog);
            fprintf(fl, "SERVER : l'operazione >Openfile< fd: %d, flag non riconosciuto %s\n", fd, req.flag);
            UNLOCK(&mlog);
            res.result = EINVAL;
          }
        }
        break;
        case READFILE: {
            res.result = readFile_coda_stor(storage_q, req.pathname, fd);

              printf("\e[0;36mSERVER : l'operazione >Readfile<, fd: %d, e' stata eseguita con esito: %s\n\e[0m", fd, myStrerror(res.result));
              LOCK(&mlog);
              fprintf(fl, "SERVER : l'operazione >Readfile<, fd: %d, e' stata eseguita con esito: %s\n", fd, myStrerror(res.result));
              UNLOCK(&mlog);

            if (res.result <= 0) return res.result;
        }
        break;
        case READNFILES: {
            res.result = readNFiles_coda_stor(storage_q, req.pathname, fd, req.datalen);

              printf("\e[0;36mSERVER : l'operazione >ReadNfiles<, fd: %d, e' stata eseguita con esito: %s\n\e[0m", fd, myStrerror(res.result));
              LOCK(&mlog);
              fprintf(fl, "SERVER : l'operazione >ReadNfiles<, fd: %d, e' stata eseguita con esito: %s\n", fd, myStrerror(res.result));
              UNLOCK(&mlog);

            if (res.result <= 0) return res.result;
        }
        break;
        case WRITEFILE: {
            void *buf = malloc(req.datalen);
            if (buf == NULL) {

                fprintf(stderr, "\e[0;36mSERVER : fd: %d, malloc WriteToFile fallita\n\e[0m", fd);
                LOCK(&mlog);
                fprintf(fl, "SERVER : fd: %d, malloc WriteToFile fallita\n", fd);
                UNLOCK(&mlog);
                fflush(stdout);
                res.result = ENOMEM;
            }

            int nread = readn(fd, buf, req.datalen);
            if (nread == 0) {


                  LOCK(&mlog);
                  fprintf(fl, "SERVER : ERRORE Worker %d, chiudo:%d\n", myid, fd);
                  UNLOCK(&mlog);
                  fflush(stdout);

                  free(buf);
                  int esito = closeFdFiles_coda_stor(storage_q, fd);
                  printf("\e[0;36mSERVER : Close fd:%d files, %s\e[0m", fd, myStrerror(esito));
                  LOCK(&mlog);
                  fprintf(fl, "SERVER : Close fd:%d files, %s", fd, myStrerror(esito));
                  UNLOCK(&mlog);
                  close(fd);

                  return -1;

            } else if (nread != req.datalen) {

                  fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE nread worker, lettura parziale\n\e[0m");
                  LOCK(&mlog);
                  fprintf(fl, "SERVER : ERRORE nread worker, lettura parziale\n");
                  UNLOCK(&mlog);
                  fflush(stderr);


                  LOCK(&mlog);
                  fprintf(fl, "SERVER : ERRORE Worker %d, chiudo:%d\n", myid, fd);
                  UNLOCK(&mlog);
                  fflush(stdout);

                  free(buf);
                  int esito = closeFdFiles_coda_stor(storage_q, fd);

                  printf("\e[0;36mSERVER : Close fd:%d files, %s\e[0m", fd, myStrerror(esito));
                  LOCK(&mlog);
                  fprintf(fl, "SERVER : Close fd:%d files, %s", fd, myStrerror(esito));
                  UNLOCK(&mlog);
                  close(fd);

                  return -1;

            } else {

                  res.result = writeFile_coda_stor(storage_q, req.pathname, fd, buf, req.datalen);
                  free(buf);

                  printf("\e[0;36mSERVER : l'operazione >WriteToFile<, fd: %d, e' stata eseguita con esito: %s\n\e[0m", fd, myStrerror(res.result));
                  LOCK(&mlog);
                  fprintf(fl, "SERVER : l'operazione >WriteToFile<, fd: %d, e' stata eseguita con esito: %s\n", fd, myStrerror(res.result));
                  UNLOCK(&mlog);

                  if (res.result <= 0) return res.result;

            }
        }
        break;
        case APPENDTOFILE: {
            void *buf = malloc(req.datalen);
            if (buf == NULL) {

                fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE fd: %d, malloc appendToFile fallita\n\e[0m", fd);
                LOCK(&mlog);
                fprintf(fl, "SERVER : ERRORE fd: %d, malloc appendToFile fallita\n", fd);
                UNLOCK(&mlog);
                fflush(stdout);
                res.result = ENOMEM;
            }

            int nread = readn(fd, buf, req.datalen);
            if (nread == 0) {




                  LOCK(&mlog);
                  fprintf(fl, "SERVER : ERRORE Worker %d, chiudo:%d\n", myid, fd);
                  UNLOCK(&mlog);

                  fflush(stdout);

                  free(buf);
                  int esito = closeFdFiles_coda_stor(storage_q, fd);
                  printf("\e[0;36mSERVER : Close fd:%d files, e' stata eseguita con esito:%s\e[0m", fd, myStrerror(esito));
                  LOCK(&mlog);
                  fprintf(fl, "SERVER : Close fd:%d files, e' stata eseguita con esito:%s", fd, myStrerror(esito));
                  UNLOCK(&mlog);
                  close(fd);

                  return -1;

            } else if (nread != req.datalen) {

                fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE nread worker, lettura parziale\n\e[0m");
                LOCK(&mlog);
                fprintf(fl, "SERVER : ERRORE nread worker, lettura parziale\n");
                UNLOCK(&mlog);
                fflush(stderr);

                LOCK(&mlog);
                fprintf(fl, "SERVER : ERRORE Worker %d, chiudo:%d\n", myid, fd);
                UNLOCK(&mlog);

                fflush(stdout);

                free(buf);
                int esito = closeFdFiles_coda_stor(storage_q, fd);


                printf("\e[0;36mSERVER : Close fd:%d files, e' stata eseguita con esito:%s\e[0m", fd, myStrerror(esito));
                LOCK(&mlog);
                fprintf(fl, "SERVER : Close fd:%d files, e' stata eseguita con esito:%s", fd, myStrerror(esito));
                UNLOCK(&mlog);

                close(fd);

                return -1;

            } else {

                  res.result = appendToFile_coda_stor(storage_q, req.pathname, fd, buf, req.datalen);
                  free(buf);

                  printf("\e[0;36mSERVER : l'operazione >AppendToFile<, fd: %d, e' stata eseguita con esito: %s\n\e[0m", fd, myStrerror(res.result));
                  LOCK(&mlog);
                  fprintf(fl, "SERVER : l'operazione >AppendToFile<, fd: %d, e' stata eseguita con esito: %s\n", fd, myStrerror(res.result));
                  UNLOCK(&mlog);

                if (res.result <= 0) return res.result;
            }
        }
        break;
        case LOCKFILE: {

               res.result = lockFile_coda_stor(storage_q, req.pathname, fd);

               printf("\e[0;36mSERVER : l'operazione >LockFile<, fd: %d, e' stata eseguita con esito: %s\n\e[0m", fd, myStrerror(res.result));
               LOCK(&mlog);
               fprintf(fl, "SERVER : l'operazione >LockFile<, fd: %d, e' stata eseguita con esito: %s\n", fd, myStrerror(res.result));
               UNLOCK(&mlog);

            if (res.result <= 0) return res.result;
        }
        break;
        case UNLOCKFILE: {

               res.result = unlockFile_coda_stor(storage_q, req.pathname, fd);

               printf("\e[0;36mSERVER : l'operazione >UnlockFile<, fd: %d, e' stata eseguita con esito: %s\n\e[0m", fd, myStrerror(res.result));
               LOCK(&mlog);
               fprintf(fl, "SERVER : l'operazione >UnlockFile<, fd: %d, e' stata eseguita con esito: %s\n", fd, myStrerror(res.result));
               UNLOCK(&mlog);

            if (res.result <= 0) return res.result;
        }
        break;
        case CLOSEFILE: {

               res.result = closeFile_coda_stor(storage_q, req.pathname, fd);

               printf("\e[0;36mSERVER : l'operazione >CloseFile<, fd: %d, e' stata eseguita con esito: %s\n\e[0m", fd, myStrerror(res.result));
               LOCK(&mlog);
               fprintf(fl, "SERVER : l'operazione >CloseFile<, fd: %d, e' stata eseguita con esito: %s\n", fd, myStrerror(res.result));
               UNLOCK(&mlog);

            if (res.result <= 0) return res.result;
        }
        break;
        case REMOVEFILE: {

              res.result = removeFile_coda_stor(storage_q, req.pathname, fd);

              printf("\e[0;36mSERVER : l'operazione >RemoveFile<, fd: %d, e' stata eseguita con esito: %s\n\e[0m", fd, myStrerror(res.result));
              LOCK(&mlog);
              fprintf(fl, "SERVER : l'operazione >RemoveFile<, fd: %d, e' stata eseguita con esito: %s\n", fd, myStrerror(res.result));
              UNLOCK(&mlog);

            if (res.result <= 0) return res.result;
        }
        break;
        default: {                                                      // non dovrebbe succedere (api scritta male)

              res.result = EPERM;

              printf("\e[0;36mSERVER : >Operazione< richiesta da fd: %d non riconosciuta\e[0m", fd);
              LOCK(&mlog);
              fprintf(fl, "SERVER : >Operazione< richiesta da fd: %d non riconosciuta", fd);
              UNLOCK(&mlog);

        }
    }

    if (writen(fd, &res, sizeof(res)) != sizeof(res)) {                 //invio messaggio al client (finisco qui nel caso in caso di errore (e fd ancora aperto))

          close(fd);

          fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE writen res requestHandler errore\n\e[0m");
          LOCK(&mlog);
          fprintf(fl, "SERVER : ERRORE writen res requestHandler errore\n");
          UNLOCK(&mlog);

          fflush(stdout);

          return -1;
    }

    fflush(stdout);
    fflush(stderr);

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

    Coda_t *q = ((threadArgs_t *)arg)->q;
    IntConLock_t *iwl = ((threadArgs_t *)arg)->iwl;
    int myid = ((threadArgs_t *)arg)->thid;
    int pfd_w = ((threadArgs_t *)arg)->pfd;                 // fd lettura pipe

    int nread;                                              // numero caratteri letti
    msg_richiesta_t req;                                    // messaggio richiesta dal client

    size_t consumed = 0;
    while (1) {

        int fd;
        fd = estrai_coda(q);
        if (fd == -1) {

              LOCK(&mlog);
              fprintf(fl, "SERVER : Worker %d, ha processato <%ld> messages\n", myid, consumed);
              UNLOCK(&mlog);
              fflush(stdout);

              return NULL;
        }

        ++consumed;

           nread = readn(fd, &req, sizeof(msg_richiesta_t));
           if (nread == 0) {

               LOCK(&mlog);
               fprintf(fl, "SERVER : ERRORE Worker %d, chiudo:%d\n", myid, fd);
               UNLOCK(&mlog);

               fflush(stdout);

               int esito = closeFdFiles_coda_stor(storage_q, fd);


               printf("\e[0;36mSERVER : Close fd: %d, e' stata eseguita con esito: %s\n\e[0m", fd, myStrerror(esito));
               LOCK(&mlog);
               fprintf(fl, "SERVER : Close fd: %d, e' stata eseguita con esito: %s\n", fd, myStrerror(esito));
               UNLOCK(&mlog);

               close(fd);
               fd = -fd;

               deleteClient(iwl);

             } else if (nread != sizeof(msg_richiesta_t)) {

               fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE nread worker, lettura parziale\n\e[0m");
               LOCK(&mlog);
               fprintf(fl, "SERVER : ERRORE nread worker, lettura parziale\n");
               UNLOCK(&mlog);
               fflush(stderr);

               LOCK(&mlog);
               fprintf(fl, "SERVER : ERRORE Worker %d, chiudo:%d\n", myid, fd);
               UNLOCK(&mlog);

               int esito = closeFdFiles_coda_stor(storage_q, fd);

              printf("\e[0;36mSERVER : Close fd: %d, esito: %s\n\e[0m", fd, myStrerror(esito));
              LOCK(&mlog);
              fprintf(fl, "SERVER : Close fd: %d, esito: %s\n", fd, myStrerror(esito));
              UNLOCK(&mlog);

              fflush(stdout);

              close(fd);
              fd = -fd;

              deleteClient(iwl);
            } else {

              printf("\e[0;36mSERVER : Il Worker %d, ha preso la richiesta : %d, da fd : %d\n\e[0m", myid, req.op, fd);
              LOCK(&mlog);
              fprintf(fl, "SERVER : Il Worker %d, ha preso la richiesta : %d, da fd : %d\n", myid, req.op, fd);
              UNLOCK(&mlog);

              fflush(stdout);

              if (requestHandler(myid, fd, req) != 0) {                           // client disconnesso

                fd = -fd;                                                         // in modo che nel main non si riaggiunga nel set
                deleteClient(iwl);
            }
        }

        if (writen(pfd_w, &fd, sizeof(fd)) != sizeof(fd)) {                       // scrittura di fd nella pipe

            fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE writen worker, parziale\n\e[0m");
            LOCK(&mlog);
            fprintf(fl, "SERVER : ERRORE writen worker, parziale\n");
            UNLOCK(&mlog);
            fflush(stderr);
            exit(-1);
        }

        memset(&req, '\0', sizeof(req));

    }

      printf("\e[0;36mSERVER : Worker %d, ha consumato <%ld> messaggi, adesso esce\n\e[0m", myid, consumed);
      LOCK(&mlog);
      fprintf(fl, "SERVER : Worker %d, ha consumato <%ld> messaggi, adesso esce\n", myid, consumed);
      UNLOCK(&mlog);

      fflush(stdout);

      return NULL;


}
