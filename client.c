#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h> /* ind AF_UNIX */
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

#include "api.h"
#include "common_def.h"
#include "common_funcs.h"
#include "client_def.h"
#include "oper_coda.h"
#include "util.h"

#define N 100


// var. globali
char sockname[N];
int time_ms = 0;                  //tempo in millisecondi tra l'invio di due richieste successive al server
int cur_dirFiles_read = 0;        // per recWrite


int gest_writeDirname(char *arg, char *dirname);

int gest_writeList(char *arg, char *dirname);

int gest_readList(char *arg, char *dirname);

int gest_readN(char *arg, char *dirname);

int gest_lockList(char *arg);

int gest_unlockList(char *arg);

int gest_removeList(char *arg);


int main(int argc, char *argv[]) {

    if (argc == 1) {
        printf("Usage: TODO\n");
        return -1;
    }

    extern char *optarg;
    int opt;
    int res = 0;

    OptQueue_t *q = opt_queue_init();
    if (!q)
    {
        fprintf(stderr, "initQueue fallita\n");
        exit(errno);
    }

    // parsing degli argomenti
    while((opt = getopt(argc, argv, "hf:w:W:D:r:R:d:t:l:u:c:p")) != -1) {
	    switch(opt) {
	        case 'h':
                printf("Usage: TODO\n");
                return 0;
	            break;
	        case 'f':
                strcpy(sockname, optarg);
	            break;
	        case 'w':
                res = opt_queue_push(q, WRITEDIRNAME, optarg);
	            break;
	        case 'W':
                res = opt_queue_push(q, WRITELIST, optarg);
	            break;
	        case 'D':
                res = opt_queue_setWDirname(q, optarg);
	            break;
	        case 'r':
                res = opt_queue_push(q, READLIST, optarg);
	            break;
	        case 'R':
                res = opt_queue_push(q, READN, optarg);
	            break;
	        case 'd':
                res = opt_queue_setRDirname(q, optarg);   //controlla se tail lista c'è READNFILES o READLIST,...se no errore
	            break;
	        case 't':
                time_ms = atoi(optarg);
                if (time_ms == 0) {
                    printf("Usage: TODO\n");
                    opt_queue_delete(q);
                    return -1;
                }
	            break;
	        case 'l':
                res = opt_queue_push(q, LOCKLIST, optarg);
	            break;
	        case 'u':
                res = opt_queue_push(q, UNLOCKLIST, optarg);
	            break;
	        case 'c':
                res = opt_queue_push(q, REMOVELIST, optarg);
	            break;
	        case 'p':
                printf("Opzione -p non ancora abilitata\n");
	            break;
	        default:
                opt_queue_delete(q);
                return -1;
	            break;
	    }

        if (res != 0) {
            opt_queue_delete(q);
            return -1;
        }

    }

    if (openConnection(sockname, 1000) == -1) { // msec = 1000 scelto arbitrariamente,
        perror("Errore : openConnection");
        return -1;
    }

    int gest_result = 0;

    // "pop" su coda
    if (q == NULL) {
        //TODO
        return -1;
    }
    while (q->head != NULL) {
        switch (q->head->opt) {
            case WRITEDIRNAME:
                printf("Richiesta scritture\n");
                gest_result = gest_writeDirname(q->head->arg, q->head->dirname);
                break;
            case WRITELIST:
                printf("Richiesta scritture\n");
                gest_result = gest_writeList(q->head->arg, q->head->dirname);
                break;
            case READLIST:
                printf("Richiesta letture\n");
                gest_result = gest_readList(q->head->arg, q->head->dirname);
                break;
            case READN:
                printf("Richiesta letture\n");
                gest_result = gest_readN(q->head->arg, q->head->dirname);
                break;
            case LOCKLIST:
                printf("Richiesta lock\n");
                gest_result = gest_lockList(q->head->arg);
                break;
            case UNLOCKLIST:
                printf("Richiesta unlock\n");
                gest_result = gest_unlockList(q->head->arg);
                break;
            case REMOVELIST:
                printf("Richiesta remove\n");
                gest_result = gest_removeList(q->head->arg);
                break;
            default:
                opt_queue_delete(q);
                return -1;
                break;
        }

        if (gest_result != 0) {
            // TODO siamo sicuri di voler uscire, al verificarsi di un errore?
            opt_queue_delete(q);
            return -1;
        }

        //fare free del nodo e delle eventuali...
        free(q->head);
        q->head = q->head->next;

        usleep(time_ms*1000);
    }

    free(q);

    if (closeConnection(sockname) == -1) {
        perror("❌ ERRORE closeConnection");

        return -1;
    }

    return 0;
}


int isdot(const char dir[]) {
  int l = strlen(dir);

  if ( (l>0 && dir[l-1] == '.') ) return 1;
  return 0;
}

int recWrite(char *readfrom_dir, char *del_dir, long n) {
    struct stat statbuf;

    printf("directory: %s\n", readfrom_dir);

    if (stat(readfrom_dir, &statbuf) != 0) {
        perror("eseguendo la stat");
		print_error("Errore in %s\n", readfrom_dir);
		return -1;
    }
    if (!S_ISDIR(statbuf.st_mode)) {
	    fprintf(stderr, "%s non e' una directory\n", readfrom_dir);
	    return -1;
    }

    DIR * dir;
    if ((dir = opendir(readfrom_dir)) == NULL) {
		perror("opendir");
		print_error("Errore aprendo la directory %s\n", readfrom_dir);
		return -1;
    } else {
        struct dirent *file;

        while((errno = 0, file = readdir(dir)) != NULL, cur_dirFiles_read < n) {
            printf("file: %s\n", file->d_name);
	        struct stat statbuf2;
            if (stat(file, &statbuf2) == -1) {
		        perror("eseguendo la stat");
		        print_error("Errore nel file %s\n", file);
		        return -1;
	        }
	        if (S_ISDIR(statbuf2.st_mode)) {
		        if (!isdot(file)) {
                    if(recWrite(file, del_dir, n) == -1) return -1;
                }
	        } else {
                cur_dirFiles_read++;

                if (openFile(file, O_CREATE_LOCK, del_dir) == -1) {
                    perror("Errore : openFile");
                    return -1;
                }
                if (writeFile(file, del_dir) == -1) {
                    perror("Errore : writeFile");
                    return -1;
                }

                if (unlockFile("token") == -1) {
                    perror("Errore : unlockFile");
                    return -1;
                }

                if (closeFile(file) == -1) {
                    perror("Errore : closeFile");
                    return -1;
                }

            }
	    }
        closedir(dir);

	    if (errno != 0) {
            perror("readdir");
            return -1;
        }
    }

    return 0;
}

int gest_writeDirname(char *arg, char *dirname) {
    if (arg == NULL) return -1;

    printf("arg: %s\n", arg);

    long n = 0;
    char *token;
    char *rest = arg;

    token = strtok_r(rest, ",", &rest);
    if (rest != NULL) {
        if (isNumber(&(rest[2]), &n) != 0)    n = 0; // client ha sbagliato a scrivere, metto n=0 (default)
    }

    // controllo se dirname è una directory
    if (dirname != NULL) {
        struct stat statbuf;
        if(stat(dirname, &statbuf) == 0) {
            if (!S_ISDIR(statbuf.st_mode)) {
	            fprintf(stderr, "%s non e' una directory\n", dirname);
	            dirname = NULL;
            }
        }
    }

    printf("token: %s\n", token);

    cur_dirFiles_read = 0;
    int rec_write_result = recWrite(token, dirname, n);
    cur_dirFiles_read = 0;

    return rec_write_result;
}


int gest_writeList(char *arg, char *dirname) {
    if (arg == NULL) return -1;

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {
        // per ciascun file:
        if (openFile("token", O_CREATE_LOCK, dirname) == -1) {
            perror("❌ ERRORE openFile");
            return -1;
        }
        if (writeFile("token", dirname) == -1) {
            perror("❌ ERRORE writeFile");
            return -1;
        }

        if (unlockFile("token") == -1) {
            perror("❌ ERRORE unlockFile");
            return -1;
        }

        if (closeFile("token") == -1) {
            perror("❌ ERRORE closeFile");
            return -1;
        }



    }

    return 0;
}


int gest_readList(char *arg, char *dirname) {
    if (arg == NULL) return -1;

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {
        // per ciascun file:
        if (openFile("token", O_NULL, NULL) == -1) {
            perror("❌ ERRORE openFile");
            return -1;
        }

        void *buf = NULL;
        size_t size = 0;
        if (readFile("token", &buf, &size) == -1) {
            perror("❌ ERRORE readFile");
            return -1;
        }

        printf("%s%n\n", buf, &size);
        fflush(stdout);

        // controllo se dirname è una directory
        if (dirname != NULL) {
            struct stat statbuf;
            if(stat(dirname, &statbuf) == 0) {
                if (S_ISDIR(statbuf.st_mode)) {
                    int res = createWriteInDir(token, buf, size, dirname);
                    free(buf);
                    return res;
                } else {
                    fprintf(stderr, "%s non e' una directory\n", dirname);
                }
            }
        }

        if (closeFile("token") == -1) {
            perror("❌ ERRORE closeFile");
            return -1;
        }


    }

    return 0;
}


int gest_readN(char *arg, char *dirname) {
    if (arg == NULL) return -1;

    long n = 0;
    if (isNumber(arg, &n) != 0)    n = 0; // client ha sbagliato a scrivere, metto n=0 (default)


    if (readNFiles(n, dirname) == -1) {
        perror("❌ ERRORE readNFiles");
        return -1;
    }



    return 0;
}


int gest_lockList(char *arg) {
    if (arg == NULL) return -1;

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {
        // per ciacun file:
        if (openFile("token", O_LOCK, NULL) == -1) {
            perror("❌ ERRORE openFile");
            return -1;
        }
        // alternativamente:
        /*
        if (openFile("token", O_NULL, NULL) == -1) {
            perror("❌ ERRORE openFile");
            return -1;
        }
        if (lockFile("token") == -1) {
            perror("❌ ERRORE lockFile");
            return -1;
        }*/



    }

    return 0;
}

int gest_unlockList(char *arg) {
    if (arg == NULL) return -1;

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) { //TODO funziona eh! la open è superflua
        // per ciacun file:
        if (openFile("token", O_NULL, NULL) == -1) {
            perror("❌ ERRORE openFile");
            return -1;
        }
        if (unlockFile("token") == -1) {
            perror("❌ ERRORE unlockFile");
            return -1;
        }

        if (closeFile("token") == -1) {
            perror("❌ ERRORE closeFile");
            return -1;
        }


    }

    return 0;
}


int gest_removeList(char *arg) {
    if (arg == NULL) return -1;

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {
        // per ciascun file:
        if (openFile("token", O_NULL, NULL) == -1) {
            perror("❌ ERRORE openFile");
            return -1;
        }

        if (removeFile("token") == -1) {
            perror("❌ ERRORE removeFile");
            return -1;
        }


    }

    return 0;
}
