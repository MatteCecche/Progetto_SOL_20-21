#define _GNU_SOURCE
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
#include <stdbool.h>

#include "../../includes/api.h"
#include "../../includes/def_tipodato.h"
#include "../../includes/fun_descrit.h"
#include "../../includes/def_client.h"
#include "../../includes/oper_coda.h"
#include "../../includes/util.h"

#define N 100


// -------------------------------------------------------------------- variabili globali -------------------------------------------------------------------- //

char sockname[N];
int time_ms = 0;                        //tempo in millisecondi tra l'invio di due richieste successive al server
int cur_dirFiles_read = 0;              // per recWrite
bool p = false;                         // opzione print
int pid;                                // process id del client




// ------------------------------------------------------------------------------------------------------------ //
//  Effettua il parsing di 'arg', ricavando il nome della cartella da cui prendere                              //
//  i file da inviare al server, ed in caso il numero di file (n).                                              //
//  Se la cartella contiene altre cartelle, queste vengono visitate ricorsivamente                              //
//  fino a quando non si leggono ‘n‘ file; se n=0 (o non è specificato) non c’è un limite                       //
//  superiore al numero di file da inviare al server (tuttavia non è detto che il server possa scriverli tutti) //
//  Se ‘dirname’ è diverso da NULL, i file eventualmente spediti dal server perchè espulsi dalla cache dovranno //
//  essere scritti in ‘dirname’                                                                                 //
// ------------------------------------------------------------------------------------------------------------ //

int gest_writeDirname(char *arg, char *dirname);

// -------------------------------------------------------------------------------------------------------------- //
//  Effettua il parsing di 'arg', ricavando la lista dei file da scrivere nel server                              //
//  Se ‘dirname’ è diverso da NULL, i file eventualmente spediti dal server perchè espulsi dalla cache dovranno   //
//  essere scritti in ‘dirname’                                                                                   //
// -------------------------------------------------------------------------------------------------------------- //


int gest_writeList(char *arg, char *dirname);

// ------------------------------------------------------------------------------------------------------------ //
//  Effettua il parsing di 'arg', ricavando il file del server a cui fare append, e il file (del file system)   //
//  da cui leggere il contenuto da aggiungere                                                                   //
//  Se ‘dirname’ è diverso da NULL, i file eventualmente spediti dal server perchè espulsi dalla cache dovranno //
//  essere scritti in ‘dirname’                                                                                 //
// ------------------------------------------------------------------------------------------------------------ //


int gest_Append(char *arg, char *dirname);

// -------------------------------------------------------------------------------- //
//  Effettua il parsing di 'arg', ricavando la lista dei file da leggere dal server //
//  Se ‘dirname’ è diverso da NULL, i file letti dal server dovranno                //
//  essere scritti in ‘dirname’                                                     //
// -------------------------------------------------------------------------------- //

int gest_readList(char *arg, char *dirname);

// ---------------------------------------------------------------------------- //
//  Effettua il parsing di 'arg', ricavando il numero di file da leggere        //
//  Se tale numero è 0,  allora vengono letti tutti i file presenti nel server  //
//  Se ‘dirname’ è diverso da NULL, i file letti dal server dovranno essere     //
//  scritti in ‘dirname’                                                        //
// ---------------------------------------------------------------------------- //

int gest_readN(char *arg, char *dirname);

// ---------------------------------------------------------------------------- //
//  Effettua il parsing di 'arg', ricavando la lista dei file su cui acquisire  //
//  la mutua esclusione                                                         //
// ---------------------------------------------------------------------------- //

int gest_lockList(char *arg);

// ------------------------------------------------------------------------------ //
//  Effettua il parsing di 'arg', ricavando la lista dei file  su cui rilasciare  //
//  la mutua esclusione                                                           //
// ------------------------------------------------------------------------------ //

int gest_unlockList(char *arg);

// ------------------------------------------------------------------------ //
//  Effettua il parsing di 'arg', ricavando la lista dei file da rimuovere  //
//  dal server se presenti                                                  //
// ------------------------------------------------------------------------ //

int gest_removeList(char *arg);

// ---------------------------- //
// Stampa le opzione possibili  //
// ---------------------------- //

static void usage(int pid);





int main(int argc, char *argv[]) {

    if (argc == 1) {

        usage(pid);

        return -1;
    }

    extern char *optarg;
    int operazione;
    int res = 0;

    pid = getpid();

    OperCoda_t *q = init_coda_oper();
    if (!q){

        fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE initQueue fallita\n\e[0m", pid);

        return -1;
    }

    while((operazione = getopt(argc, argv, ":h:f:w:W:D:a:A:r:R:d:t:l:u:c:p")) != -1) {    // parsing degli argomenti ed opportuna costruzione della coda delle operazioni
	    switch(operazione) {
	        case 'h':
                usage(pid);
                return 0;
	            break;
	        case 'f':
                strcpy(sockname, optarg);
	            break;
	        case 'w':
                res = ins_coda_oper(q, WRITEDIRNAME, optarg);
	            break;
	        case 'W':
                res = ins_coda_oper(q, WRITELIST, optarg);
	            break;
	        case 'D':
                res = setWDirname_coda_oper(q, optarg);
	            break;
	        case 'a':
                res = ins_coda_oper(q, APPEND, optarg);
	            break;
	        case 'A':
                res = setADirname_coda_oper(q, optarg);
	            break;
	        case 'r':
                res = ins_coda_oper(q, READLIST, optarg);
	            break;
	        case 'R':
                if (optarg != NULL) {
                    if (optarg[0] != '-') {
                        res = ins_coda_oper(q, READN, optarg);
                    } else {
                        optarg = argv[optind--];
                        res = ins_coda_oper(q, READN, "n=0");
                    }

                } else {
                    res = ins_coda_oper(q, READN, "n=0");
                }
	            break;
	        case 'd':
                res = setRDirname_coda_oper(q, optarg);                         //controlla se tail lista c'è READNFILES o READLIST
	            break;
	        case 't':
                time_ms = atoi(optarg);
                if (time_ms == 0) {
                    fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE -t richiede numero \n\e[0m", pid);
                    usage(pid);
                    canc_coda_oper(q);
                    return -1;
                }
	            break;
	        case 'l':
                res = ins_coda_oper(q, LOCKLIST, optarg);
	            break;
	        case 'u':
                res = ins_coda_oper(q, UNLOCKLIST, optarg);
	            break;
	        case 'c':
                res = ins_coda_oper(q, REMOVELIST, optarg);
	            break;
	        case 'p':
                p = true;
	            break;
          case ':':
          switch (optopt){
                case 'R':
                    res = ins_coda_oper(q, READN, "n=0");
                break;
                default:
                    usage(pid);
                    canc_coda_oper(q);
                    return -1;
                }
                break;
	        default:
                usage(pid);
                canc_coda_oper(q);
                return -1;
	            break;
	    }

        if (res != 0) {

            canc_coda_oper(q);
            printf("\e[0;32mCLIENT %d: Operation coda \e[0;31mERRORE\n\e[0m", pid);

            return -1;
        }

    }

    struct timespec current_time;
    if (clock_gettime(CLOCK_REALTIME, &current_time) == -1) {

        canc_coda_oper(q);

        return -1;
    }
    current_time.tv_sec = current_time.tv_sec + 10;                           //scelto arbitrariamente
    current_time.tv_nsec = current_time.tv_nsec + 0;
    if (openConnection(sockname, 1000, current_time) == -1) {                 // msec = 1000 scelto arbitrariamente

        fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE openConnection %s, \e[0;31mERRORE %s\n\e[0m", pid, sockname, strerror(errno));
        canc_coda_oper(q);

        return -1;
    }

    int gest_result = 0;

    if (q == NULL) {                                                          // "pop" su coda

        printf("\e[0;32mCLIENT %d: Nessuna richiesta rilevata\n\e[0m", pid);

        return 0;
    }
    while (q->head != NULL) {
        switch (q->head->opt) {
            case WRITEDIRNAME:
                printf("\e[0;32mCLIENT %d: Richiesta scritture\n\e[0m", pid);
                gest_result = gest_writeDirname(q->head->arg, q->head->dirname);
                break;
            case WRITELIST:
                printf("\e[0;32mCLIENT %d: Richiesta scritture\n\e[0m", pid);
                gest_result = gest_writeList(q->head->arg, q->head->dirname);
                break;
            case APPEND:
                printf("\e[0;32mCLIENT %d: Richiesta append\n\e[0m", pid);
                gest_result = gest_Append(q->head->arg, q->head->dirname);
                break;
            case READLIST:
                printf("\e[0;32mCLIENT %d: Richiesta lettura\n\e[0m", pid);
                gest_result = gest_readList(q->head->arg, q->head->dirname);
                break;
            case READN:
                printf("\e[0;32mCLIENT %d: Richiesta lettura\n\e[0m", pid);
                gest_result = gest_readN(q->head->arg, q->head->dirname);
                break;
            case LOCKLIST:
                printf("\e[0;32mCLIENT %d: Richiesta lock\n\e[0m", pid);
                gest_result = gest_lockList(q->head->arg);
                break;
            case UNLOCKLIST:
                printf("\e[0;32mCLIENT %d: Richiesta unlock\n\e[0m", pid);
                gest_result = gest_unlockList(q->head->arg);
                break;
            case REMOVELIST:
                printf("\e[0;32mCLIENT %d: Richiesta remove\n\e[0m", pid);
                gest_result = gest_removeList(q->head->arg);
                break;
            default:
                canc_coda_oper(q);
                return -1;
                break;
        }

        if (gest_result != 0) {

            canc_coda_oper(q);

            return -1;
        }


        OperNodo_t *temp = q->head;                                             //fare free del nodo
        q->head = q->head->next;
        free(temp);

        usleep(time_ms*1000);
    }

    free(q);

    if (closeConnection(sockname) == -1) {

        fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE closeConnection\e[0m", pid);

        return -1;
    }

    return 0;
}


// ------------------------------------------------------------ //
// Controlla se il primo carattere della stringa 'dir' è un '.' //
// ------------------------------------------------------------ //

int isdot(const char dir[]) {

    if (dir[0] == '.') return 1;

    return 0;

}

// ------------------------------------------------------------------------------------------------------------ //
//  Visita 'readfrom_dir' ricorsivamente fino a quando non si leggono ‘n‘ file; se n=0 non c’è un limite        //
//  superiore al numero di file da inviare al server (tuttavia non è detto che il server possa scriverli tutti) //
//  Se 'del_dir' è diverso da NULL, i file eventualmente spediti dal server perchè espulsi dalla cache dovranno //
//  essere scritti in ‘del_dir’                                                                                 //
// ------------------------------------------------------------------------------------------------------------ //


int recWrite(char *readfrom_dir, char *del_dir, long n) {

    struct stat statbuf;

    if (stat(readfrom_dir, &statbuf) != 0) {

        fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeDirname stat %s, ERRORE %s\n\e[0m", pid, readfrom_dir, strerror(errno));

        return -1;
    }

    if (!S_ISDIR(statbuf.st_mode)) {

        fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeDirname , %s is not a directory\n\e[0m", pid, readfrom_dir);

        return -1;
    }

    DIR * dir;
    if ((dir = opendir(readfrom_dir)) == NULL) {

        fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeDirname opendir %s, ERRORE %s\n\e[0m", pid, readfrom_dir, strerror(errno));

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

                fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeDirname, ERRORE : UNIX_PATH_MAX troppo piccolo\n\e[0m", pid);

                return -1;
	        }

	        strncpy(filename, readfrom_dir, PATH_MAX-1);
	        strncat(filename,"/", PATH_MAX-1);
	        strncat(filename,file->d_name, PATH_MAX-1);

            if (stat(filename, &statbuf2) == -1) {

                fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeDirname stat %s, ERRORE %s\n\e[0m", pid, filename, strerror(errno));

                return -1;
	        }

	        if (S_ISDIR(statbuf2.st_mode)) {

                if(recWrite(filename, del_dir, n) == -1) return -1;
	        } else {
                cur_dirFiles_read++;

                if (openFile(filename, O_CREATE_LOCK, del_dir) == -1) {

                    fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeDirname openFile %s, ERRORE %s\n\e[0m", pid, filename, strerror(errno));

                    return -1;
                }
                if (writeFile(filename, del_dir) == -1) {

                    fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeDirname writeFile %s, ERRORE %s\n\e[0m", pid, filename, strerror(errno));

                    return -1;
                }


                if (closeFile(filename) == -1) {                                                                              //fa anche unlock

                    fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeDirname closeFile %s, ERRORE %s\n\e[0m", pid, filename, strerror(errno));

                    return -1;
                }

                if (p) printf("\e[0;32mCLIENT %d: writeDirname %s, SUCCESSO\n\e[0m", pid, filename);

            }
	    }

        closedir(dir);

	    if (errno != 0) {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeDirname readdir, ERRORE %s\n\e[0m", pid, strerror(errno));

            return -1;
        }
    }

    return 0;
}

int gest_writeDirname(char *arg, char *dirname) {

    if (arg == NULL) {

        fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeDirname, ERRORE invalid arg\n\e[0m", pid);

        return -1;
    }

    long n = 0;
    char *token;
    char *rest = arg;

    token = strtok_r(rest, ",", &rest);
    if (rest != NULL) {

        if (isNumber(&(rest[2]), &n) != 0)    n = 0;     // client ha sbagliato a scrivere, metto n=0 (default)
    }


    if (dirname != NULL && dirname[0] != '\0') {        // controllo se dirname è una directory
        struct stat statbuf;
        if(stat(dirname, &statbuf) == 0) {

            if (!S_ISDIR(statbuf.st_mode)) {

                fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeDirname, %s is not a directory\n\e[0m", pid, dirname);
	              dirname = NULL;
            }
        } else {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeDirname stat %s, ERRORE %s\n\e[0m", pid, dirname, strerror(errno));
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

        fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeList, ERRORE invalid arg\n\e[0m", pid);

        return -1;
    }

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {

        if (openFile(token, O_CREATE_LOCK, dirname) == -1) {                    // per ciascun file:

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeList openFile %s, ERRORE %s\n\e[0m", pid, token, strerror(errno));

            return -1;
        }

        if (writeFile(token, dirname) == -1) {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeList writeFile %s, ERRORE %s\n\e[0m", pid, token, strerror(errno));

            return -1;
        }


        if (closeFile(token) == -1) {                                             //fa la unlock anche

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE writeList closeFile %s, ERRORE %s\n\e[0m", pid, token, strerror(errno));

            return -1;
        }

        if (p) printf("\e[0;32mCLIENT %d: writeDirname %s, SUCCESSO\n\e[0m", pid, token);
    }

    return 0;

}


int gest_Append(char *arg, char *dirname) {

    if (arg == NULL) {

        fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE Append, ERRORE invalid arg\n\e[0m", pid);

        return -1;
    }

    char *token;
    char *rest = arg;

    if (token = strtok_r(rest, ",", &rest)) {

        if (rest == NULL) {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE Append, ERRORE invalid source file\n\e[0m", pid);

            return -1;
        }

        if (openFile(token, O_NULL, dirname) == -1) {                             // per ciascun file:

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE Append openFile %s, ERRORE %s\n\e[0m", pid, token, strerror(errno));

            return -1;
        }

        FILE *file;
        void *file_out = NULL;
        unsigned long file_size = 0;

        if ((file = fopen(rest,"r")) == NULL) {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE Append fopen %s, ERRORE %s\n\e[0m", pid, rest, strerror(errno));

            return -1;
        }


        fseek(file, 0L, SEEK_END);                                                // Scopriamo la dimensione del file
        file_size = ftell(file);
        rewind(file);

        file_out = (void *)malloc(file_size);
        if (file_out == NULL) {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE Append malloc fallita\n\e[0m", pid);
            fclose(file);

            return -1;
        } else {

            size_t bytes_read = fread(file_out, 1, file_size, file);
            if (bytes_read != file_size) {

                fclose(file);
                free(file_out);
                errno = EIO;
                fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE Append fread %s, ERRORE %s\n\e[0m", pid, rest, strerror(errno));

                return -1;
            }
        }
        fclose(file);

        if (appendToFile(token, file_out, file_size, dirname) == -1) {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE Append appendToFile %s %lu bytes, ERRORE %s\n\e[0m", pid, token, file_size, strerror(errno));
            free(file_out);

            return -1;
        }

        free(file_out);

        if (closeFile(token) == -1) {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE Append closeFile %s, ERRORE %s\n\e[0m", pid, token, strerror(errno));

            return -1;
        }

        if (p) printf("\e[0;32mCLIENT %d: Append al file %s %lu bytes, SUCCESSO\n\e[0m", pid, token, file_size);

    } else return -1;

    return 0;
}


int gest_readList(char *arg, char *dirname) {

    if (arg == NULL) return -1;

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {

        if (openFile(token, O_NULL, NULL) == -1) {                              // per ciascun file:

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE readList openFile %s, ERRORE %s\n\e[0m", pid, token, strerror(errno));

            return -1;
        }

        void *buf = NULL;
        size_t size = 0;
        if (readFile(token, &buf, &size) == -1) {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE readList readFile %s, ERRORE %s\n\e[0m", pid, token, strerror(errno));

            return -1;
        }

        if (dirname != NULL && dirname[0] != '\0') {                            // controllo se dirname è una directory
            struct stat statbuf;
            if(stat(dirname, &statbuf) == 0) {

                if (S_ISDIR(statbuf.st_mode)) {

                    if (createWriteInDir(token, buf, size, dirname) != 0) {

                        free(buf);

                        return -1;
                    }
                } else {

                    fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE readList , %s is not a directory\n\e[0m", pid, dirname);
                }
            } else {

                fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE readList stat %s, ERRORE %s\n\e[0m", pid, dirname, strerror(errno));
                dirname = NULL;
            }
        }

        free(buf);

        if (closeFile(token) == -1) {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE readList closeFile %s, ERRORE %s\n\e[0m", pid, token, strerror(errno));

            return -1;
        }

        if (p) printf("\e[0;32mCLIENT %d: readList file %s %lu bytes, SUCCESSO\n\e[0m", pid, token, size);

    }

    return 0;
}


int gest_readN(char *arg, char *dirname) {

    if (arg == NULL) return -1;

    long n = 0;
    if (strlen(arg) > 2 && isNumber(&(arg[2]), &n) != 0)    n = 0; // client ha sbagliato a scrivere, metto n=0 (default)

    if (readNFiles(n, dirname) == -1) {

        fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE readN readNFiles %ld, ERRORE %s\n\e[0m", pid, n, strerror(errno));

        return -1;
    }

    if (p) printf("\e[0;32mCLIENT %d: readN readNFiles SUCCESSO\n\e[0m", pid);

    return 0;
}


int gest_lockList(char *arg) {

    if (arg == NULL) return -1;

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {

        if (openFile(token, O_LOCK, NULL) == -1) {                              // per ciacun file:

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE lockList openFile %s, ERRORE %s\n\e[0m", pid, token, strerror(errno));

            return -1;
        }

        if (p) printf("\e[0;32mCLIENT %d: lockList %s SUCCESSO\n\e[0m", pid, token);

    }

    return 0;
}

int gest_unlockList(char *arg) {

    if (arg == NULL) return -1;

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {

        if (unlockFile(token) == -1) {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE unlockList unlockFile %s, ERRORE %s\n\e[0m", pid, token, strerror(errno));

            return -1;
        }

        if (closeFile(token) == -1) {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE unlockList closeFile %s, ERRORE %s\n\e[0m", pid, token, strerror(errno));

            return -1;
        }

        if (p) printf("\e[0;32mCLIENT %d: unlockList %s SUCCESSO\n\e[0m", pid, token);

    }

    return 0;
}


int gest_removeList(char *arg) {

    if (arg == NULL) return -1;

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {

        if (openFile(token, O_LOCK, NULL) == -1) {                              // per ciascun file:

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE removeList openFile %s, ERRORE %s\n\e[0m", pid, token, strerror(errno));

            return -1;
        }

        if (removeFile(token) == -1) {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE removeList removeFile %s, ERRORE %s\n\e[0m", pid, token, strerror(errno));

            return -1;
        }

        if (p) printf("\e[0;32mCLIENT %d: removeList %s SUCCESSO\n\e[0m", pid, token);

    }

    return 0;
}


static void usage(int pid) {

    printf("\e[0;32mCLIENT %d: usage:\n\e[0m"
	    "\e[0;32m | -h : stampa la lista di tutte le opzioni accettate dal client\n\e[0m"
	    "\e[0;32m | -f filename : specifica il nome del socket AF_UNIX a cui connettersi\n\e[0m"
	    "\e[0;32m | -w dirname[,n=0] : invia al server i file nella cartella 'dirname'\n\e[0m"
	    "\e[0;32m | -W file1[,file2]: lista di nomi di file da scrivere nel server separati da ','\n\e[0m"
	    "\e[0;32m | -D dirname : cartella in memoria secondaria dove vengono scritti (lato client) i file che il server rimuove\n\e[0m"
	    "\e[0;32m | -a : file,file2: file del server a cui fare appende e file (del file system) da cui leggere il contenuto da aggiungere\n\e[0m"
	    "\e[0;32m | -A dirname : cartella in memoria secondaria dove vengono scritti (lato client) i file che il server rimuove a casusa dell'append\n\e[0m"
	    "\e[0;32m | -r file1[,file2] : lista di nomi di file da leggere dal server separati da ','\n\e[0m"
	    "\e[0;32m | -R [n=0] : legge n file qualsiasi memorizzati nel server; n=0 (o non è specificato) vengono letti tutti i file presenti nel server\n\e[0m"
	    "\e[0;32m | -d dirname : cartella in memoria secondaria dove scrivere i file letti dal server con l'opzione '-r' o '-R'\n\e[0m"
	    "\e[0;32m | -t time : ritardo in millisecondi che di due richieste successive al server (se non specificata si suppone -t 0)\n\e[0m"
	    "\e[0;32m | -l file1[,file2] : lista di nomi di file su cui acquisire la mutua esclusione\n\e[0m"
	    "\e[0;32m | -u file1[,file2] : lista di nomi di file su cui rilasciare la mutua esclusione\n\e[0m"
	    "\e[0;32m | -c file1[,file2] : lista di file da rimuovere dal server se presenti\n\e[0m"
	    "\e[0;32m | -p : stampa per ogni operazione: tipo di operazione, file di riferimento, esito e bytes letti/scritti\n\e[0m",
	    pid);
}
