#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H
#include <pthread.h>
#include "def_tipodato.h"

// ------------------------------------------------------------------------------------ //
// Crea (in modalità detached) il thread che gestisce i segnali (e gli passa signalArg) //
// ------------------------------------------------------------------------------------ //

pthread_t createSignalHandlerThread(signalThreadArgs_t *signalArg, FILE *l, pthread_mutex_t ml);

#endif
