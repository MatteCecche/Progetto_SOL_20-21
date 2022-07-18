#ifndef CODA_STORAGE_H
#define CODA_STORAGE_H

#include <pthread.h>
#include <stdbool.h>
#include <limits.h>

#include "def_tipodato.h"
#include "coda.h"

// ------------------------------------------------------------ Elemento della coda storage ------------------------------------------------------------ //

typedef struct StorageNode {

    char 	              pathname[PATH_MAX];	      // nome completo del file (univoco nello storage)
	  void* 	            data;	                    // dati del file
	  int  	              len;		                  // lunghezza del file
	  bool	              locked;		                // flag di lock da client
	  int	                fd_locker;	              // fd del processo client che ha fatto lock
    bool                locker_can_write;         // true se l'ultima operazione fatta dal locker è una open con create_lock
    Coda_t              *opener_q;                // coda dei client (fd) che hanno aperto il file
    pthread_cond_t      filecond;                 // locked && fd_locker != fd
	  struct StorageNode  *next;

} NodoStorage_t;


// ------------------------------------------------------------ Struttura dati coda storage ------------------------------------------------------------ //

typedef struct StorageQueue {

    NodoStorage_t       *head;                    // elemento di testa
    NodoStorage_t       *tail;                    // elemento di coda
    int                 cur_numfiles;             // lunghezza ovvero numero di files in storage
    unsigned long       cur_usedstorage;          // dimensione storage attuale in bytes
    int                 max_num_files; 			      // numero massimo raggiunto di file memorizzati nel server
    unsigned long       max_used_storage; 	      // dimensione massima in bytes raggiunta dal file storage;
    int                 replace_occur;			      // numero di volte in cui l’algoritmo di rimpiazzamento della cache è stato eseguito
    int                 limit_num_files;          // numero limite di file nello storage
    unsigned long       storage_capacity;         // dimensione dello storage in bytes
    pthread_mutex_t     qlock;

} CodaStorage_t;


// ---------------------------------------------------------------- //
//  Alloca ed inizializza una coda. Deve essere chiamata da un solo //
//  thread (tipicamente il thread main).                            //
// ---------------------------------------------------------------- //

CodaStorage_t *init_coda_stor(int limit_num_files, unsigned long storage_capacity, FILE *l, pthread_mutex_t ml);


// ------------------------------------------ //
// Sblocca tutti i thread bloccati su le wait //
// ------------------------------------------ //

void broadcast_coda_stor(CodaStorage_t *q);


// -------------------------------------------------------------- //
// Cancella una coda allocata con initCoda. Deve essere chiamata  //
// da un solo thread (tipicamente il thread main).                //
// -------------------------------------------------------------- //

void canc_coda_stor(CodaStorage_t *q);


// ------------------------------------------------------------------------------ //
// Inserisce un nuovo nodo (file identificato da pathname) nella coda             //
// (inizializzando correttamente i vari campi), ed in caso di successo comunica   //
// al client sul socket fd_locker                                                 //
// ------------------------------------------------------------------------------ //

int ins_coda_stor(CodaStorage_t *q, char *pathname, bool locked, int fd_locker, FILE *l, pthread_mutex_t ml);


// ------------------------------------------------------------------------------ //
//  Aggiorna gli opener del nodo, (file) della coda ,identificato da pathname     //
//  (inizializzando correttamente i vari campi), ed in caso di successo comunica  //
//  al client sul socket fd_locker                                                //
//  (se un altro client ha il lock su quel file, aspetta (wait))                  //
// ------------------------------------------------------------------------------ //

int updateOpeners_coda_stor(CodaStorage_t *q, char *pathname, bool locked, int fd, FILE *l, pthread_mutex_t ml);


// -----------------------------------------------------------------------------//
//  Cerca nella coda il file identificato da pathname, nel caso lo trovasse, lo //
//  comunica al client (sul socket fd)                                          //
// ---------------------------------------------------------------------------- //

int readFile_coda_stor(CodaStorage_t *q, char *pathname, int fd);


// ------------------------------------------------------------------------------ //
//  Comunica al client (sul socket fd_locker), i files contenuti nella coda, fino //
//  ad un massimo di n (se n<=0 comunica tutti i file contenuti nella coda)       //
// ------------------------------------------------------------------------------ //

int readNFiles_coda_stor(CodaStorage_t *q, char *pathname, int fd, int n);


// ------------------------------------------------------------------------------------------- //
//  Cerca nella coda il file identificato da pathname (controlla che l'ultima                  //
//  operazione sul file sia una open con i flag create e lock), copia il file puntato da buf   //
//  ed in caso di successo comunica al client sul socket fd                                    //
// ------------------------------------------------------------------------------------------- //

int writeFile_coda_stor(CodaStorage_t *q, char *pathname, int fd, void *buf, int buf_len);


// ------------------------------------------------------------------------------------------------------ //
//  Cerca nella coda il file identificato da pathname, aggiunge ad esso in append il file puntato da buf  //
//  ed in caso di successo comunica al client sul socket fd                                               //
// ------------------------------------------------------------------------------------------------------ //

int appendToFile_coda_stor(CodaStorage_t *q, char *pathname, int fd, void *buf, int buf_len);


// ------------------------------------------------------------------------------------------ //
//  Cerca di prendere il lock (cioè fd_locker = fd) sul Nodo (file) idetificato da pathname,  //
//  in caso di successo comunica al client sul socket fd                                      //
//  (se un altro client ha il lock su quel file, aspetta (wait))                              //
// ------------------------------------------------------------------------------------------ //

int lockFile_coda_stor(CodaStorage_t *q, char *pathname, int fd);


// -------------------------------------------------------------------------- //
// Rilascia il lock, ed in caso di successo comunica al client sul socket fd  //
// -------------------------------------------------------------------------- //

int unlockFile_coda_stor(CodaStorage_t *q, char *pathname, int fd);


// ---------------------------------------------------------------------------------------------- //
//  Client fd chiude il file identificato da pathname (si aggiorna opencoda), se lo aveva aperto, //
//  in caso di successo comunica al client sul socket fd                                          //
// ---------------------------------------------------------------------------------------------- //


int closeFile_coda_stor(CodaStorage_t *q, char *pathname, int fd);


// ---------------------------------------------------------------- //
//  Chiude (aggiornando i loro opencoda) tutti i file aperti da fd, //
//  in caso di successo comunica al client sul socket fd            //
// ---------------------------------------------------------------- //

int closeFdFiles_coda_stor(CodaStorage_t *q, int fd);


// ------------------------------------------------------ //
//  Rimuove il file identificato da pathname dalla coda,  //
//  in caso di successo comunica al client sul socket fd  //
// ------------------------------------------------------ //

int removeFile_coda_stor(CodaStorage_t *q, char *pathname, int fd);


// ---------------------------------------------- //
// Stampa la lista dei file contenuti nella coda  //
// ---------------------------------------------- //

void printListFiles_coda_stor(CodaStorage_t *q);


// ---------------------------------------------------------------  //
// Ritorna la lunghezza attuale della coda passata come parametro.  //
// ---------------------------------------------------------------- //

unsigned long lung_coda_stor(CodaStorage_t *q);



#endif
