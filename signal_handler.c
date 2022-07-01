#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  //api posix
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h> //dominio AF_UNIX
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "signal_handler.h"
#include "config.h"

extern struct config_struct config;


// Thread che gestisce i segnali (cambia lo stato del server, inoltre tramite pipe (chiudendola) sveglia il server nel caso fosse bloccato su select)
void *SignalHandler(void *arg) {

  signalThreadArgs_t *puntatore = (signalThreadArgs_t*) arg;

  fprintf(stdout," #%d: ## signal thread partito ##\n",getpid());

  server_status * status = puntatore->status;
  int pfd_w = puntatore->pfd_w;

  /* costruisco la maschera */
  sigset_t set;
  int sig;
  sigemptyset(&set);              //inizializza il set di segnali impostato per escludere tutti i segnali definiti.
  sigaddset(&set, SIGINT);        //aggiunge il signum del segnale al set di segnali
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGHUP);

  /* blocco */
  sigprocmask(SIG_BLOCK, &set, NULL);     //modificala maschera del segnale

  while(*status == RUNNING) {
    sigwait(&set, &sig);   //seleziona un segnale in sospeso dall'insieme e lo cancella dall'insieme di segnali in sospeso
    switch (sig) {
      case SIGINT:
          if (config.v > 1) printf("SignalHandler: Ricevuto SigInt\n");
          fflush(stdout);
          *status = CLOSED;
          break;
      case SIGQUIT:
          if (config.v > 1) printf("SignalHandler: Ricevuto SigQuit\n");
          *status = CLOSED;
          break;
      case SIGHUP:
          if (config.v > 1) printf("SignalHandler: Ricevuto SigHup\n");
          *status = CLOSING;
          break;
      default:
          break;
      }
  }

    close(pfd_w);

    return NULL;
}


pthread_t createSignalHandlerThread(signalThreadArgs_t *signalArg) {
    sigset_t set;
	sigfillset(&set);	 //inizializza il set di segnali impostato per includere tutti i segnali definiti.
    /* blocco tutti i segnali prima dell'avvio del signal handler  */

    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {   //modificala maschera del segnale
        perror("🤖  SERVER:❌ ERRORE signal_set (SignalHandler");
        return NULL;
    }


    /* signal handler thread */
    pthread_t tid;
    if(pthread_create(&tid, NULL, SignalHandler, signalArg) != 0) {
        fprintf(stderr, "🤖  SERVER: ❌ ERRORE pthread_create failed (SignalHandler)\n");
        return NULL;
    }

    pthread_detach(tid);

    return tid;
}
