#define _GNU_SOURCE
#define PATH_MAX        4096
#ifndef OPER_CODA_H
#define OPER_CODA_H

#include <pthread.h>
#include <limits.h>
#include "def_client.h"


// ------------------------------------------------------ Elemento della coda delle operazioni ------------------------------------------------------ //

typedef struct Opt {

    input_opt   opt;                          // enum operazione
    char        arg[PATH_MAX];                // argomento operazione
    char        dirname[PATH_MAX];            // eventuale directory per opt. di read/write/append
    struct Opt  *next;

} OperNodo_t;

// ------------------------------------------------------ Struttura dati coda delle operazioni ------------------------------------------------------//

typedef struct OperCoda {

    OperNodo_t        *head;    // elemento di testa
    OperNodo_t        *tail;    // elemento di coda
    unsigned long      qlen;    // lunghezza

} OperCoda_t;


// ------------------------------------------------------------ //
// Tutte le operazioni devono essere chiamate da un solo thread //
// ------------------------------------------------------------ //


/** Alloca ed inizializza una coda
 *
 *   \retval NULL se si sono verificati problemi nell'allocazione
 *   \retval puntatore alla coda allocata
 */
OperCoda_t *init_coda_oper();

/** Cancella una coda allocata con init_coda_oper.
 *
 *   \param q puntatore alla coda da cancellare
 */
void canc_coda_oper(OperCoda_t *q);

/** Inserisce un nuovo nodo (operazione) nella coda
 *
 *   \param q puntatore alla coda
 *   \param opt operazione da inserire
 *   \param arg argomento operazione
 *
 *   \retval 0 se successo
 *   \retval errno se errore
 */
int ins_coda_oper(OperCoda_t *q, int opt, char *arg);

/** Setta q->dirname =  dirname, se l'operazione contenuta nella tail della coda è WRITEDIRNAME o WRITELIST
 *
 *   \param q puntatore alla coda
 *   \param dirname nome della directory (dove vengono scritti i file rimossi dal server)
 *
 *   \retval 0 se successo
 *   \retval errno se errore
 */
int setWDirname_coda_oper(OperCoda_t *q, char *dirname);

/** Setta q->dirname =  dirname, se l'operazione contenuta nella tail della coda è READLIST o READN
 *
 *   \param q puntatore alla coda
 *   \param dirname nome della directory (dove vengono scritti i file letti dal server)
 *
 *   \retval 0 se successo
 *   \retval errno se errore
 */
int setRDirname_coda_oper(OperCoda_t *q, char *dirname);

/** Setta q->dirname =  dirname, se l'operazione contenuta nella tail della coda è APPEND
 *
 *   \param q puntatore alla coda
 *   \param dirname nome della directory (dove vengono scritti i file rimossi dal server)
 *
 *   \retval 0 se successo
 *   \retval errno se errore
 */
int setADirname_coda_oper(OperCoda_t *q, char *dirname);


#endif
