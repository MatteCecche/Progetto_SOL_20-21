#ifndef CLIENT_DEF_H
#define CLIENT_DEF_H

typedef enum {

    WRITEDIRNAME = 1,   // -w
    WRITELIST = 2,      // -W
    APPEND = 3,         // -a
    READLIST = 4,       // -r
    READN = 5,          // -R
    LOCKLIST = 6,       // -l
    UNLOCKLIST = 7,     // -u
    REMOVELIST = 8      // -r

} input_opt;


int isdot(const char dir[]);


// Controlla se il primo carattere della stringa 'dir' è un '.'


int recWrite(char *readfrom_dir, char *del_dir, long n);


//  Visita 'readfrom_dir' ricorsivamente fino a quando non si leggono ‘n‘ file; se n=0 non c’è un limite
//  superiore al numero di file da inviare al server (tuttavia non è detto che il server possa scriverli tutti)
//  Se 'del_dir' è diverso da NULL, i file eventualmente spediti dal server perchè espulsi dalla cache dovranno
//  essere scritti in ‘del_dir’



int gest_writeDirname(char *arg, char *dirname);


//   Effettua il parsing di 'arg', ricavando il nome della cartella da cui prendere
//   i file da inviare al server, ed in caso il numero di file (n).
//   Se la cartella contiene altre cartelle, queste vengono visitate ricorsivamente
//   fino a quando non si leggono ‘n‘ file; se n=0 (o non è specificato) non c’è un limite
//   superiore al numero di file da inviare al server (tuttavia non è detto che il server possa scriverli tutti)
//   Se ‘dirname’ è diverso da NULL, i file eventualmente spediti dal server perchè espulsi dalla cache dovranno
//   essere scritti in ‘dirname’


int gest_writeList(char *arg, char *dirname);


//  Effettua il parsing di 'arg', ricavando la lista dei file da scrivere nel server
//  Se ‘dirname’ è diverso da NULL, i file eventualmente spediti dal server perchè espulsi dalla cache dovranno
//  essere scritti in ‘dirname’


int gest_Append(char *arg, char *dirname);


//  Effettua il parsing di 'arg', ricavando il file del server a cui fare append, e il file (del file system)
//  da cui leggere il contenuto da aggiungere
//  Se ‘dirname’ è diverso da NULL, i file eventualmente spediti dal server perchè espulsi dalla cache dovranno
//  essere scritti in ‘dirname’


int gest_readList(char *arg, char *dirname);


//Effettua il parsing di 'arg', ricavando la lista dei file da leggere dal server
//  Se ‘dirname’ è diverso da NULL, i file letti dal server dovranno
//  essere scritti in ‘dirname’


int gest_readN(char *arg, char *dirname);


//  Effettua il parsing di 'arg', ricavando il numero di file da leggere
//  Se tale numero è 0,  allora vengono letti tutti i file presenti nel server
//  Se ‘dirname’ è diverso da NULL, i file letti dal server dovranno essere
//  scritti in ‘dirname’


int gest_lockList(char *arg);


//  Effettua il parsing di 'arg', ricavando la lista dei file su cui acquisire
//  la mutua esclusione


int gest_unlockList(char *arg);

//  Effettua il parsing di 'arg', ricavando la lista dei file  su cui rilasciare
//  la mutua esclusione


int gest_removeList(char *arg);

//  Effettua il parsing di 'arg', ricavando la lista dei file da rimuovere
//  dal server se presenti


static void usage(int pid);

// Stampa le opzione possibili


#endif
