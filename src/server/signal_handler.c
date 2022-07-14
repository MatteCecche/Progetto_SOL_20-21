#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>         //api posix
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>         //dominio AF_UNIX
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "../../includes/signal_handler.h"
#include "../../includes/config.h"
#include "../../includes/util.h"

extern struct config_struct config;
FILE *fl2;
pthread_mutex_t mlog2;

// -------------------------------------------------------------------------------------------------------------------------------------------------- //
// Thread che gestisce i segnali (cambia lo stato del server, inoltre tramite pipe (chiudendola) sveglia il server nel caso fosse bloccato su select) //
// -------------------------------------------------------------------------------------------------------------------------------------------------- //

void *SignalHandler(void *arg) {


    signalThreadArgs_t *puntatore = (signalThreadArgs_t*) arg;

    fprintf(stdout,"\e[0;36m%d : ########## signal thread partito ##########\n\e[0m",getpid());
    LOCK(&mlog2);
    fprintf(fl2, "%d : ########## signal thread partito ##########\n",getpid());
    UNLOCK(&mlog2);

    server_status * status = puntatore->status;
    int pfd_w = puntatore->pfd_w;

    sigset_t set;         // costruisco la maschera
    int sig;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
	  sigaddset(&set, SIGQUIT);
	  sigaddset(&set, SIGHUP);

    sigprocmask(SIG_BLOCK, &set, NULL);

    while(*status == RUNNING) {
        sigwait(&set, &sig);

        switch (sig) {
            case SIGINT:
                if (config.v > 1){
                  printf("\e[0;36mSERVER : SignalHandler: Ricevuto SigInt\n\e[0m");
                  LOCK(&mlog2);
                  fprintf(fl2, "SERVER : SignalHandler: Ricevuto SigInt\n");
                  UNLOCK(&mlog2);
                }
                fflush(stdout);
                *status = CLOSED;
                break;
            case SIGQUIT:
                if (config.v > 1){
                  printf("\e[0;36mSERVER : SignalHandler: Ricevuto SigQuit\n\e[0m");
                  LOCK(&mlog2);
                  fprintf(fl2, "SERVER : SignalHandler: Ricevuto SigQuit\n");
                  UNLOCK(&mlog2);
                }
                *status = CLOSED;
                break;
            case SIGHUP:
                if (config.v > 1){
                  printf("\e[0;36mSERVER : SignalHandler: Ricevuto SigHup\n\e[0m");
                  LOCK(&mlog2);
                  fprintf(fl2, "SERVER : SignalHandler: Ricevuto SigHup\n");
                  UNLOCK(&mlog2);
                }
                *status = CLOSING;
                break;
            default:
                break;
        }
    }

    close(pfd_w);

    return NULL;

}


pthread_t createSignalHandlerThread(signalThreadArgs_t *signalArg, FILE *l, pthread_mutex_t ml) {

    fl2 = l;
    mlog2 = ml;
    sigset_t set;
	  sigfillset(&set);

    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {                             // blocco tutti i segnali prima dell'avvio del signal handler

        perror("\e[0;36mSERVER : \e[0;31mERRORE signal_set (SignalHandler\e[0m");
        LOCK(&mlog2);
        fprintf(fl2, "SERVER : ERRORE signal_set (SignalHandler");
        UNLOCK(&mlog2);

        return NULL;
    }


    pthread_t tid;
    if(pthread_create(&tid, NULL, SignalHandler, signalArg) != 0) {

        fprintf(stderr, "\e[0;36mSERVER : \e[0;31mERRORE pthread_create failed (SignalHandler)\n\e[0m");
        LOCK(&mlog2);
        fprintf(fl2, "SERVER : ERRORE pthread_create failed (SignalHandler)\n");
        UNLOCK(&mlog2);

        return NULL;
    }

    pthread_detach(tid);

    return tid;

}
