#include <pthread.h>

// Elemento coda

typedef struct Node {

  int    data;
  struct Node * next;

} Nodo;

//Struttura coda

typedef struct Queue {

  Nodo        *head;    // elemento di testa
  Nodo        *tail;    // elemento di coda
  unsigned long  qlen;    // lunghezza
  pthread_mutex_t qlock;
  pthread_cond_t  qcond;

} Coda;


Coda *init_coda();                        //Alloca ed inizializza una coda


void canc_coda(Coda *q);                  //Cancella una coda allocata con queue_init


int ins_coda(Coda *q, int data);          // Inserisce un dato nella coda


int estrai_coda(Coda *q);                 // Estrae un dato dalla coda


Nodo* trova_coda(Coda *q, int fd);      //Cerca fd nella coda


int cancella_nodo_coda(Coda *q, int fd);  // Rimuove (se c'è) fd dalla coda


unsigned long lung_coda(Coda *q);         // Ritorna la lunghezza della coda
