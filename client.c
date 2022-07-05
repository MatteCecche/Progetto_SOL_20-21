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


// variabili globali
char sockname[N];
int time_ms = 0;                //tempo in millisecondi tra l'invio di due richieste successive al server
int cur_dirFiles_read = 0;      // per recWrite
bool p = false;                 // opzione print
int pid;                        // process id del client



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


int main(int argc, char *argv[]) {

    if (argc == 1) {
        usage(pid);
        return -1;
    }

    extern char *optarg;
    int opt;
    int res = 0;

    pid = getpid();

    Oper_Coda_t *q = oper_coda_init();

    if (!q){
        fprintf(stderr, "%d: initCoda fallita\n", pid);
        return -1;
    }

    // parsing degli argomenti ed opportuna costruzione della coda delle operazioni
    while((opt = getopt(argc, argv, ":hf:w:W:D:a:A:r:R:d:t:l:u:c:p")) != -1) {
	    switch(opt) {
	        case 'h':
                usage(pid);
                return 0;
	            break;
	        case 'f':
                strcpy(sockname, optarg);
	            break;
	        case 'w':
                res = oper_coda_ins(q, WRITEDIRNAME, optarg);
	            break;
	        case 'W':
                res = oper_coda_ins(q, WRITELIST, optarg);
	            break;
	        case 'D':
                res = oper_coda_setWDirname(q, optarg);
	            break;
	        case 'a':
                res = oper_coda_ins(q, APPEND, optarg);
	            break;
	        case 'A':
                res = oper_coda_setADirname(q, optarg);
	            break;
	        case 'r':
                res = oper_coda_ins(q, READLIST, optarg);
	            break;
	        case 'R':
                if (optarg != NULL) {
                    printf("optarg: %s\n",optarg);
                    if (optarg[0] != '-') {
                        res = oper_coda_ins(q, READN, optarg);
                    } else {
                        optarg = argv[optind--];
                        res = oper_coda_ins(q, READN, "n=0");
                    }

                } else {
                    res = oper_coda_ins(q, READN, "n=0");
                }
	            break;
	        case 'd':
                res = oper_coda_setRDirname(q, optarg);   //controlla se tail lista c'è READNFILES o READLIST,...se no errore
	            break;
	        case 't':
                time_ms = atoi(optarg);
                if (time_ms == 0) {
                    fprintf(stderr, "👦 %d: ❌ -t number required\n", pid);
                    usage(pid);
                    oper_coda_canc(q);
                    return -1;
                }
	            break;
	        case 'l':
                res = oper_coda_ins(q, LOCKLIST, optarg);
	            break;
	        case 'u':
                res = oper_coda_ins(q, UNLOCKLIST, optarg);
	            break;
	        case 'c':
                res = oper_coda_ins(q, REMOVELIST, optarg);
	            break;
	        case 'p':
                p = true;
	            break;
            case ':':
                switch (optopt)
                {
                case 'R':
                    res = oper_coda_ins(q, READN, "n=0");

                    break;
                default:
                    usage(pid);
                    oper_coda_canc(q);
                    return -1;
                }
                break;
	        default:
                usage(pid);
                oper_coda_canc(q);
                return -1;
	            break;
	    }

        if (res != 0) {
            oper_coda_canc(q);
            printf("%d: Errore operazione coda\n", pid);
            return -1;
        }

    }

    struct timespec current_time;

    if (clock_gettime(CLOCK_REALTIME, &current_time) == -1) {
        oper_coda_canc(q);
        return -1;
    }
    current_time.tv_sec = current_time.tv_sec + 10;                   //scelto arbitrariamente (passare come opzione in millisecondi volendo, e trasformarlo in sec e nanosecm)
    current_time.tv_nsec = current_time.tv_nsec + 0;

    if (openConnection(sockname, 1000, current_time) == -1) {         // msec = 1000 scelto arbitrariamente
        fprintf(stderr, "👦 %d: ❌ openConnection %s, ERRORE %s\n", pid, sockname, strerror(errno));
        oper_coda_canc(q);

        return -1;
    }

    int gest_result = 0;

    // "pop" su coda
    if (q == NULL) {
        printf("%d: Nessuna richiesta rilevata\n", pid);
        return 0;
    }
    while (q->head != NULL) {
        switch (q->head->opt) {
            case WRITEDIRNAME:
                printf("%d: Richiesta scritture\n", pid);
                gest_result = gest_writeDirname(q->head->arg, q->head->dirname);
                break;
            case WRITELIST:
                printf("%d: Richiesta scritture\n", pid);
                gest_result = gest_writeList(q->head->arg, q->head->dirname);
                break;
            case APPEND:
                printf("%d: Richiesta append\n", pid);
                gest_result = gest_Append(q->head->arg, q->head->dirname);
                break;
            case READLIST:
                printf("%d: Richiesta letture\n", pid);
                gest_result = gest_readList(q->head->arg, q->head->dirname);
                break;
            case READN:
                printf("%d: Richiesta letture\n", pid);
                gest_result = gest_readN(q->head->arg, q->head->dirname);
                break;
            case LOCKLIST:
                printf("%d: Richiesta lock\n", pid);
                gest_result = gest_lockList(q->head->arg);
                break;
            case UNLOCKLIST:
                printf("%d: Richiesta unlock\n", pid);
                gest_result = gest_unlockList(q->head->arg);
                break;
            case REMOVELIST:
                printf("%d: Richiesta remove\n", pid);
                gest_result = gest_removeList(q->head->arg);
                break;
            default:
                oper_coda_canc(q);
                return -1;
                break;
        }

        if (gest_result != 0) {
            oper_coda_canc(q);
            return -1;
        }

        //free del nodo
        OperNodo_t *temp = q->head;
        q->head = q->head->next;
        free(temp);

        usleep(time_ms*1000);
    }

    free(q);

    if (closeConnection(sockname) == -1) {
        fprintf(stderr, "%d: Errore closeConnection", pid);
        return -1;
    }

    return 0;
}



// Controlla se il primo carattere della stringa 'dir' è un '.'

int isdot(const char dir[]) {
    if (dir[0] == '.') return 1;
    return 0;

//  Visita 'readfrom_dir' ricorsivamente fino a quando non si leggono ‘n‘ file; se n=0 non c’è un limite
//  superiore al numero di file da inviare al server (tuttavia non è detto che il server possa scriverli tutti)
//  Se 'del_dir' è diverso da NULL, i file eventualmente spediti dal server perchè espulsi dalla cache dovranno
//  essere scritti in ‘del_dir’


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
    if (rest != NULL) {
        if (isNumber(&(rest[2]), &n) != 0)    n = 0; // client ha sbagliato a scrivere, metto n=0 (default)
    }

    // controllo se dirname è una directory
    if (dirname != NULL && dirname[0] != '\0') {
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
        fprintf(stderr, "👦 %d: ❌ writeList, ERRORE invalid arg\n", pid);
        return -1;
    }

    char *token;
    char *rest = arg;

    while (token = strtok_r(rest, ",", &rest)) {
        // per ciascun file:
        if (openFile(token, O_CREATE_LOCK, dirname) == -1) {
            fprintf(stderr, "%d: writeList openFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        if (writeFile(token, dirname) == -1) {
            fprintf(stderr, "%d: writeList writeFile %s, ERRORE %s\n", pid, token, strerror(errno));
            return -1;
        }

        if (closeFile(token) == -1) {         //fa la unlock anche
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

        // per ciascun file:
        if (openFile(token, O_NULL, dirname) == -1) {
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

        // Scopriamo la dimensione del file
        fseek(file, 0L, SEEK_END);
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
                // Some kind of I/O error
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

    while (token = strtok_r(rest, ",", &rest)) {
        // per ciascun file:
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

        // controllo se dirname è una directory
        if (dirname != NULL && dirname[0] != '\0') {
            struct stat statbuf;
            if(stat(dirname, &statbuf) == 0) {
                if (S_ISDIR(statbuf.st_mode)) {
                    if (createWriteInDir(token, buf, size, dirname) != 0) {
                        free(buf);
                        return -1;
                    }
                } else {
                    fprintf(stderr, "%d: readList , %s is not a directory\n", pid, dirname);
                }
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
    if (strlen(arg) > 2 && isNumber(&(arg[2]), &n) != 0)    n = 0; // client ha sbagliato a scrivere, metto n=0 (default)

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

    while (token = strtok_r(rest, ",", &rest)) {
        // per ciacun file:
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

    printf("%d: usage:\n"
	    "  -h : stampa la lista di tutte le opzioni accettate dal client\n"
	    "  -f filename : specifica il nome del socket AF_UNIX a cui connettersi\n"
	    "  -w dirname[,n=0] : invia al server i file nella cartella 'dirname'\n"
	    "  -W file1[,file2]: lista di nomi di file da scrivere nel server separati da ','\n"
	    "  -D dirname : cartella in memoria secondaria dove vengono scritti (lato client) i file che il server rimuove\n"
	    "  -a : file,file2: file del server a cui fare appende e file (del file system) da cui leggere il contenuto da aggiungere"
	    "  -A dirname : cartella in memoria secondaria dove vengono scritti (lato client) i file che il server rimuove a casusa dell'append\n"
	    "  -r file1[,file2] : lista di nomi di file da leggere dal server separati da ','\n"
	    "  -R [n=0] : legge n file qualsiasi memorizzati nel server; n=0 (o non è specificato) vengono letti tutti i file presenti nel server\n"
	    "  -d dirname : cartella in memoria secondaria dove scrivere i file letti dal server con l'opzione '-r' o '-R'\n"
	    "  -t time : ritardo in millisecondi che di due richieste successive al server (se non specificata si suppone -t 0)\n"
	    "  -l file1[,file2] : lista di nomi di file su cui acquisire la mutua esclusione\n"
	    "  -u file1[,file2] : lista di nomi di file su cui rilasciare la mutua esclusione\n"
	    "  -c file1[,file2] : lista di file da rimuovere dal server se presenti\n"
	    "  -p : stampa per ogni operazione: tipo di operazione, file di riferimento, esito e bytes letti/scritti\n",
	    pid);
}
