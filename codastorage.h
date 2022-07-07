#include "coda.h"
#include <pthread.h>
#include <stdbool.h>
#include <limits.h>
#include "common_def.h"

typedef struct StorageNode {

  char 	          pathname[PATH_MAX];	  // nome completo del file (univoco nello storage)
	void* 	        data;	                // dati del file
	int  	          len;		              // lunghezza del file
	bool	          locked;		            // flag di lock da client
	int	            fd_locker;	          // fd del processo client che ha fatto lock
  bool            locker_can_write;     // true se l'ultima operazione fatta dal locker è una open con create_lock
  Coda            *opener_q;            // coda dei client (fd) che hanno aperto il file
  pthread_cond_t  filecond;             // locked && fd_locker != fd
	struct StorageNode* next;

} NodoStorage;

typedef struct StorageQueue {

  NodoStorage*  head;                 // elemento di testa
  NodoStorage*  tail;                 // elemento di coda
  int             cur_numfiles;       // lunghezza ovvero numero di files in storage
  unsigned long   cur_usedstorage;    //dimensione storage attuale in bytes
  int max_num_files; 			            // numero massimo raggiunto di file memorizzati nel server
  unsigned long max_used_storage; 	  // dimensione massima in bytes raggiunta dal file storage;
  int replace_occur;			            // numero di volte in cui l’algoritmo di rimpiazzamento della cache è stato eseguito
  int limit_num_files;                // numero limite di file nello storage
  unsigned long storage_capacity;     // dimensione dello storage in bytes
  pthread_mutex_t qlock;

} CodaStorage;


NodoStorage* trova_coda_s(CodaStorage *q, char *pathname);

// Restituisce il nodo identificato da pathname

int queue_s_popUntil(CodaStorage *q, int buf_len, int *num_poppedFiles, NodoStorage **poppedHead);

// Estrae dalla coda q un numero di files adeguato per permettere l'inserimento di un file di buf_len bytes,
//  non superando la capacità dello storage in bytes
//  (da chiamare con lock (su q))

NodoStorage *cancella_coda_s(CodaStorage *q);

// Estrae dalla coda q un nodo (file)
//  (da chiamare con lock)

int send_to_client_and_free(int fd, int num_files, NodoStorage *poppedH);

// Comunica al client sul socket fd, i files della coda poppedH, e libera memoria

CodaStorage *init_coda_s(int limit_num_files, unsigned long storage_capacity);

// Alloca ed inizializza una coda.

void broadcast_coda_s(CodaStorage *q);

// Sblocca tutti i thread bloccati su le wait

void canc_coda_s(CodaStorage *q);

// Cancella una coda allocata con initQueue.

int ins_coda_s(CodaStorage *q, char *pathname, bool locked, int fd_locker);

// Inserisce un nuovo nodo (file identificato da pathname) nella coda

int aggiornaOpeners_coda_s(CodaStorage *q, char *pathname, bool locked, int fd);

// Aggiorna gli opener del nodo, (file) della coda ,identificato da pathname

int readFile_coda_s(CodaStorage *q, char *pathname, int fd);

// Cerca nella coda il file identificato da pathname, nel caso lo trovasse, lo comunica al client (sul socket fd)

int readNFile_coda_s(CodaStorage *q, char *pathname, int fd, int n);

// Comunica al client (sul socket fd_locker), i files contenuti nella coda, fino ad un massimo di n (se n<=0 comunica tutti i file contenuti nella coda)

int writeFile_coda_s(CodaStorage *q, char *pathname, int fd, void *buf, int buf_len);

// Cerca nella coda il file identificato da pathname (controlla che l'ultima
//  operazione sul file sia una open con i flag create e lock), copia il file puntato da buf
//  ed in caso di successo comunica al client sul socket fd

int appendToFile_coda_s(CodaStorage *q, char *pathname, int fd, void *buf, int buf_len);

// Cerca nella coda il file identificato da pathname, aggiunge ad esso in append il file puntato da buf ed in caso di successo comunica al client sul socket fd

int lockFile_coda_s(CodaStorage *q, char *pathname, int fd);

// Cerca di prendere il lock (cioè fd_locker = fd) sul node (file) idetificato da pathname,
//  in caso di successo comunica al client sul socket fd
//  (se un altro client ha il lock su quel file, aspetta (wait))

int unlockFile_coda_s(CodaStorage *q, char *pathname, int fd);

// Rilascia il lock, ed in caso di successo comunica al client sul socket fd

int closeFile_coda_s(CodaStorage *q, char *pathname, int fd);

// Client fd chiude il file identificato da pathname (si aggiorna opener_q), se lo aveva aperto,
//  in caso di successo comunica al client sul socket fd

int closeFdFile_coda_s(CodaStorage *q, int fd);

// Chiude (aggiornando i loro opener_q) tutti i file aperti da fd, in caso di successo comunica al client sul socket fd

int removeFile_coda_s(CodaStorage *q, char *pathname, int fd);

// Rimuove il file identificato da pathname dalla coda,
//  in caso di successo comunica al client sul socket fd

void stampalistaFile_coda_s(CodaStorage *q);

// Stampa la lista dei file contenuti nella coda

unsigned long lung_coda_s(CodaStorage *q);

// Ritorna la lunghezza attuale della coda passata come parametro.
