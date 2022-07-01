#define CONFIG_LINE_BUFFER_SIZE 128             //lunghezza massima riga config
#define MAX_CONFIG_VARIABLE_LEN 32              //lunghezza massima del nome di una variabile nel file di configurazione
#define UNIX_PATH_MAX 104


// parametri che definiscono la struttura del server //

struct config_struct {

  int             num_workers; 			            // numero di workers (config)
  char 	          sockname[UNIX_PATH_MAX]; 	    // socket file
  int             limit_num_files;	            // numero limite di file nello storage (config)
  unsigned long   storage_capacity;	            // dimensione dello storage in bytes (config)
  int             info;                         // quantita' info da print (v)

};


void init_config_file(char* config_filename);