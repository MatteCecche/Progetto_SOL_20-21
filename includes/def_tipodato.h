#define _GNU_SOURCE
#define PATH_MAX        4096
#ifndef DEF_TIPODATO_H
#define DEF_TIPODATO_H

#include <limits.h>

#define UNIX_PATH_MAX 104

// ------------------------------------------------------------------------ stato del server ------------------------------------------------------------------------ //

typedef enum {

    RUNNING = 0,
    CLOSING = 1,
    CLOSED = 2

} server_status;

// ------------------------------------ tipo di dato usato per passare gli argomenti al thread signal handler -------------------------------------------------------- //

typedef struct signalThreadArgs{

    int pfd_w;
    server_status *status;

} signalThreadArgs_t;

// ------------------------------------ flag con cui può essere richiesta l'apertura di un file ----------------------------------------------------------------------- //

typedef enum {

    O_NULL        = 0,
    O_CREATE      = 1,
    O_LOCK        = 2,
    O_CREATE_LOCK = 3

} file_flag;

// ------------------------------------------------------------------------ tipo di operazione sul server -------------------------------------------------------------- //

typedef enum {

    NOTHING = 0,
    OPENFILE = 1,
    READFILE = 2,
    READNFILES = 3,
    WRITEFILE = 4,
    APPENDTOFILE = 5,
    LOCKFILE = 6,
    UNLOCKFILE = 7,
    CLOSEFILE = 8,
    REMOVEFILE = 9

} op_t;

// ------------------------------------------------------------------------ struttura del messaggio di richiesta al server ---------------------------------------------- //

typedef struct {

    op_t       op;                               // tipo di operazione sul server
    char       pathname[PATH_MAX];               // nome del file
    int        flag;                             // flag con cui può essere richiesta l'apertura di un file
    int        datalen;                          // assume un significato diverso a seconda di 'op':
                                                 //     - READNFILES: numero di files che richiediamo di leggere
                                                 //     - WRITEFILE, APPENDTOFILE : lunghezza in byte del file che invieremo subito dopo
                                                 //     - altrimenti superfluo (=0)
} msg_richiesta_t;

// ------------------------------------------------------------------------ struttura del messaggio di risposta dal server ---------------------------------------------- //

typedef struct {

    int   result;                   // 0:success, >0: errno
    char  pathname[PATH_MAX];       // nome del file (eventualmente)
    int   datalen;

} msg_risposta_t;


#endif
