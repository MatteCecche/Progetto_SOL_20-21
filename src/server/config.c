#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../includes/config.h"


// ----------------------- variabili di configurazione in una struct globale, definita in server.c ----------------------- //

extern struct config_struct config;

int read_int_from_config_line(char* config_line) {

	char prm_name[MAX_CONFIG_VARIABLE_LEN];
	int val;
	const char ch = '=';
	char *ret;
	ret = strchr(config_line, ch);
	sscanf((char *)ret, "%s %d\n", prm_name, &val);

	return val;

}

void read_str_from_config_line(char* config_line, char* val) {

	char prm_name[MAX_CONFIG_VARIABLE_LEN];
	const char ch = '=';
	char *ret;
	ret = strchr(config_line, ch);
	sscanf((char *)ret, "%s %s\n", prm_name, val);

}

void read_config_file(char* config_filename) {

	FILE *fp;
	char buf[CONFIG_LINE_BUFFER_SIZE];

	if ((fp=fopen(config_filename, "r")) == NULL){

		fprintf(stderr, ">>Failed to open config file %s", config_filename);
		exit(EXIT_FAILURE);
 	}
   	while(! feof(fp)) {

				fgets(buf, CONFIG_LINE_BUFFER_SIZE, fp);
      	if (buf[0] == '#' || strlen(buf) < 4) {

				continue;
      	}

      	if (strstr(buf, "Num_workers ")) {

						config.num_workers = read_int_from_config_line(buf);
      	}
      	if (strstr(buf, "Sockname ")) {

						read_str_from_config_line(buf, config.sockname);
      	}
      	if (strstr(buf, "LimitNumFiles ")) {

						config.limit_num_files = read_int_from_config_line(buf);
     	}
      	if (strstr(buf, "StorageCapacity ")) {

						config.storage_capacity = read_int_from_config_line(buf);
      	}
		if (strstr(buf, "PrintLevel ")) {

						config.v = read_int_from_config_line(buf);
      	}

	}

	fclose(fp);


}
