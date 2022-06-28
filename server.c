#include <stdio.h>
#include <string.h>
#include "config.h"

struct config_struct config;

int main(int argc, char *argv[]){

  if (argc != 3 || strcmp(argv[1], "-c")) {
    fprintf(stderr, "File config non trovato o non inserito correttamente\n");
    return -1;
  }

  init_config_file(argv[2]);  //inizializzo i dati

  printf("config verbosity: %d\n", config.info);

    if (config.info > 2) {
        printf("config.num_workers: %d\n", config.num_workers);
        printf("config.sockname: %s\n", config.sockname);
        printf("config.limit_num_files: %d\n", config.limit_num_files);
        printf("config.storage_capacity: %lu\n", config.storage_capacity);
    }

}
