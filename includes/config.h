#ifndef DEF_TIPODATO_H
#include "def_tipodato.h"
#endif

#if !defined(CONFIG_H)
#define CONFIG_H

#define CONFIG_LINE_BUFFER_SIZE 128   //lunghezza massima di una riga del file di configurazione
#define MAX_CONFIG_VARIABLE_LEN 32    //lunghezza massima del nome di una variabile nel file di configurazione


// ------------------------------------------------ struttura contenente le info di configurazione per il server ------------------------------------------------  //

struct config_struct {

    int             num_workers; 			                // numero di workers (config)
    char 	          sockname[UNIX_PATH_MAX]; 	        // socket file
    int             limit_num_files;	                // numero limite di file nello storage (config)
    unsigned long   storage_capacity;	                // dimensione dello storage in bytes (config)
    char            path_filelog[UNIX_PATH_MAX];      // percorso dove si trova il filelog

};


// ------------------------------------------------------------------------------------------------------------------------------------------ //
// Legge il file di configurazione e imposta le variabili di 'config' (variabile globale extern di tipo struct config_struct) nella struttura //
// ------------------------------------------------------------------------------------------------------------------------------------------ //

void read_config_file(char* config_filename);



#endif
