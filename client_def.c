#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include "api.h"
#include "util.h"
#include "client_def.h"
#include "common_def.h"
#include "common_funcs.h"


int isdot(const char dir[]) {

    if (dir[0] == '.') return 1;
    return 0;

}

int recWrite(char *readfrom_dir, char *del_dir, long n) {

    struct stat statbuf;

    if (stat(readfrom_dir, &statbuf) != 0) {

        fprintf(stderr, "%d: writeDirname stat %s, ERRORE %s\n", pid, readfrom_dir, strerror(errno));
		    return -1;
    }
    if (!S_ISDIR(statbuf.st_mode)) {

        fprintf(stderr, "%d: writeDirname , %s is not a directory\n", pid, readfrom_dir);
	      return -1;
    }

    DIR * dir;
    if ((dir = opendir(readfrom_dir)) == NULL) {

        fprintf(stderr, "%d: writeDirname opendir %s, ERRORE %s\n", pid, readfrom_dir, strerror(errno));
		    return -1;
    } else {
        struct dirent *file;

        while((errno = 0, file = readdir(dir)) != NULL && (cur_dirFiles_read < n || n == 0)) {

            if (isdot(file->d_name)) continue;

	          struct stat statbuf2;
            char filename[PATH_MAX];
	          int len1 = strlen(readfrom_dir);
	          int len2 = strlen(file->d_name);
	          if ((len1+len2+2)>PATH_MAX) {

                fprintf(stderr, "%d: writeDirname, ERRORE : UNIX_PATH_MAX troppo piccolo\n", pid);
                return -1;
	          }

	          strncpy(filename, readfrom_dir, PATH_MAX-1);
	          strncat(filename,"/", PATH_MAX-1);
	          strncat(filename,file->d_name, PATH_MAX-1);

            if (stat(filename, &statbuf2) == -1) {

                fprintf(stderr, "%d: writeDirname stat %s, ERRORE %s\n", pid, filename, strerror(errno));
		            return -1;
	          }

	          if (S_ISDIR(statbuf2.st_mode)) {

                if(recWrite(filename, del_dir, n) == -1) return -1;
	          } else {
                cur_dirFiles_read++;

                if (openFile(filename, O_CREATE_LOCK, del_dir) == -1) {

                    fprintf(stderr, "%d: writeDirname openFile %s, ERRORE %s\n", pid, filename, strerror(errno));
                    return -1;
                }
                if (writeFile(filename, del_dir) == -1) {

                    fprintf(stderr, "%d:  writeDirname writeFile %s, ERRORE %s\n", pid, filename, strerror(errno));
                    return -1;
                }

                if (closeFile(filename) == -1) {        //fa anche unlock

                    fprintf(stderr, "%d: writeDirname closeFile %s, ERRORE %s\n", pid, filename, strerror(errno));
                    return -1;
                }

                if (p) printf("%d: writeDirname %s, SUCCESSO\n", pid, filename);

            }
	    }

        closedir(dir);

	    if (errno != 0) {

            fprintf(stderr, "%d: writeDirname readdir, ERRORE %s\n", pid, strerror(errno));
            return -1;
      }
    }

    return 0;

}


int gest_writeDirname(char *arg, char *dirname) {

    if (arg == NULL) {

        fprintf(stderr, "%d: writeDirname, ERRORE invalid arg\n", pid);
        return -1;
    }

    long n = 0;
    char *token;
    char *rest = arg;

    token = strtok_r(rest, ",", &rest);
    if (rest != NULL) if (isNumber(&(rest[2]), &n) != 0)  n = 0;  // client ha sbagliato a scrivere, metto n=0 (default)

    if (dirname != NULL && dirname[0] != '\0') {                  // controllo se dirname è una directory

        struct stat statbuf;
        if(stat(dirname, &statbuf) == 0) {

            if (!S_ISDIR(statbuf.st_mode)) {

                fprintf(stderr, " %d: writeDirname, %s is not a directory\n", pid, dirname);
	              dirname = NULL;
            }
        } else {

            fprintf(stderr, "%d: writeDirname stat %s, ERRORE %s\n", pid, dirname, strerror(errno));
            dirname = NULL;
        }
    }

    cur_dirFiles_read = 0;
    int rec_write_result = recWrite(token, dirname, n);
    cur_dirFiles_read = 0;

    return rec_write_result;

}


int gest_writeList(char *arg, char *dirname) {

    if (arg == NULL) {

        fprintf(stderr, "%d: writeList, ERRORE invalid arg\n", pid);
        return -1;
    }

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {        // per ciascun file:

        if (openFile(token, O_CREATE_LOCK, dirname) == -1) {

            fprintf(stderr, "%d: writeList openFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        if (writeFile(token, dirname) == -1) {

            fprintf(stderr, "%d: writeList writeFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        if (closeFile(token) == -1) {         //fa la unlock

            fprintf(stderr, "%d: writeList closeFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        if (p) printf("%d: writeDirname %s, SUCCESSO\n", pid, token);
    }

    return 0;
}


int gest_Append(char *arg, char *dirname) {

    if (arg == NULL) {

        fprintf(stderr, "%d: Append, ERRORE invalid arg\n", pid);
        return -1;
    }

    char *token;
    char *rest = arg;

    if (token = strtok_r(rest, ",", &rest)) {

        if (rest == NULL) {

            fprintf(stderr, "%d: Append, ERRORE invalid source file\n", pid);
            return -1;
        }

        if (openFile(token, O_NULL, dirname) == -1) {         // per ciascun file:

            fprintf(stderr, "%d: Append openFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        FILE *file;
        void *file_out = NULL;
        unsigned long file_size = 0;

        if ((file = fopen(rest,"r")) == NULL) {

            fprintf(stderr, "%d: Append fopen %s, ERRORE %s\n", pid, rest, strerror(errno));
            return -1;
        }


        fseek(file, 0L, SEEK_END);        // Scopriamo la dimensione del file
        file_size = ftell(file);
        rewind(file);

        file_out = (void *)malloc(file_size);
        if (file_out == NULL) {

            fprintf(stderr, "%d: Append malloc fallita\n", pid);
            fclose(file);
            return -1;
        } else {
            size_t bytes_read = fread(file_out, 1, file_size, file);
            if (bytes_read != file_size) {

                fclose(file);
                free(file_out);
                errno = EIO;
                fprintf(stderr, "%d: Append fread %s, ERRORE %s\n", pid, rest, strerror(errno));
                return -1;
            }
        }
        fclose(file);

        if (appendToFile(token, file_out, file_size, dirname) == -1) {

            fprintf(stderr, "%d: Append appendToFile %s %lu bytes, ERRORE %s\n", pid, token, file_size, strerror(errno));
            free(file_out);
            return -1;
        }

        free(file_out);

        if (closeFile(token) == -1) {

            fprintf(stderr, "%d: Append closeFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        if (p) printf("%d: Append al file %s %lu bytes, SUCCESSO\n", pid, token, file_size);

    } else return -1;

    return 0;
}


int gest_readList(char *arg, char *dirname) {

    if (arg == NULL) return -1;

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {    // per ciascun file:

        if (openFile(token, O_NULL, NULL) == -1) {

            fprintf(stderr, "%d: readList openFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        void *buf = NULL;
        size_t size = 0;
        if (readFile(token, &buf, &size) == -1) {

            fprintf(stderr, "%d: readList readFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }


        if (dirname != NULL && dirname[0] != '\0') {      // controllo se dirname è una directory

            struct stat statbuf;
            if(stat(dirname, &statbuf) == 0) {

                if (S_ISDIR(statbuf.st_mode)) {

                    if (createWriteInDir(token, buf, size, dirname) != 0) {

                        free(buf);
                        return -1;
                    }
                } else fprintf(stderr, "%d: readList , %s is not a directory\n", pid, dirname);
            } else {

                fprintf(stderr, "%d: readList stat %s, ERRORE %s\n", pid, dirname, strerror(errno));
                dirname = NULL;
            }
        }

        free(buf);

        if (closeFile(token) == -1) {

            fprintf(stderr, "%d: readList closeFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        if (p) printf("%d: readList file %s %lu bytes, SUCCESSO\n", pid, token, size);

    }

    return 0;
}


int gest_readN(char *arg, char *dirname) {

    if (arg == NULL) return -1;

    long n = 0;
    if (strlen(arg) > 2 && isNumber(&(arg[2]), &n) != 0)    n = 0;      // client ha sbagliato a scrivere, metto n=0 (default)

    if (readNFiles(n, dirname) == -1) {

        fprintf(stderr, "%d:readN readNFiles %ld, ERRORE %s\n", pid, n, strerror(errno));
        return -1;
    }

    if (p) printf("%d: readN readNFiles SUCCESSO\n", pid);

    return 0;
}


int gest_lockList(char *arg) {

    if (arg == NULL) return -1;

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {        // per ciacun file:

        if (openFile(token, O_LOCK, NULL) == -1) {
            fprintf(stderr, "%d: lockList openFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        if (p) printf("%d: lockList %s SUCCESSO\n", pid, token);

    }

    return 0;
}

int gest_unlockList(char *arg) {

    if (arg == NULL) return -1;

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {

        if (unlockFile(token) == -1) {

            fprintf(stderr, "%d: unlockList unlockFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        if (closeFile(token) == -1) {

            fprintf(stderr, "%d: unlockList closeFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        if (p) printf("%d: unlockList %s SUCCESSO\n", pid, token);

    }

    return 0;
}


int gest_removeList(char *arg) {

    if (arg == NULL) return -1;

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {
        // per ciascun file:
        if (openFile(token, O_LOCK, NULL) == -1) {

            fprintf(stderr, " %d: removeList openFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        if (removeFile(token) == -1) {

            fprintf(stderr, "%d: removeList removeFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        if (p) printf("%d: removeList %s SUCCESSO\n", pid, token);

    }

    return 0;
}


static void usage(int pid) {

    printf(  "  %d: usage:\n",pid);
	  printf(  "  -h : stampa la lista di tutte le opzioni accettate dal client\n");
	  printf(  "  -f filename : specifica il nome del socket AF_UNIX a cui connettersi\n");
	  printf(  "  -w dirname[,n=0] : invia al server i file nella cartella 'dirname'\n");
	  printf(  "  -W file1[,file2]: lista di nomi di file da scrivere nel server separati da ','\n");
	  printf(  "  -D dirname : cartella in memoria secondaria dove vengono scritti (lato client) i file che il server rimuove\n");
	  printf(  "  -r file1[,file2] : lista di nomi di file da leggere dal server separati da ','\n");
	  printf(  "  -R [n=0] : legge n file qualsiasi memorizzati nel server; n=0 (o non è specificato) vengono letti tutti i file presenti nel server\n");
	  printf(  "  -d dirname : cartella in memoria secondaria dove scrivere i file letti dal server con l'opzione '-r' o '-R'\n");
	  printf(  "  -t time : ritardo in millisecondi che di due richieste successive al server (se non specificata si suppone -t 0)\n");
	  printf(  "  -l file1[,file2] : lista di nomi di file su cui acquisire la mutua esclusione\n");
	  printf(  "  -u file1[,file2] : lista di nomi di file su cui rilasciare la mutua esclusione\n");
	  printf(  "  -c file1[,file2] : lista di file da rimuovere dal server se presenti\n");
	  printf(  "  -p : stampa per ogni operazione: tipo di operazione, file di riferimento, esito e bytes letti/scritti\n");
}
