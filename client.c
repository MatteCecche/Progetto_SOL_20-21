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




int main(int argc, char *argv[]) {

    if (argc == 1) {

        usage(pid);
        return -1;
    }

    extern char *optarg;
    int opt;
    int res = 0;

    pid = getpid();

    Oper_Coda_t *q = oper_coda_init();          //inizializzo la coda delle operazioni

    if (!q){

        fprintf(stderr, "%d: initCoda fallita\n", pid);
        return -1;
    }

    // parsing degli argomenti ed opportuna costruzione della coda delle operazioni
    while((opt = getopt(argc, argv, ":h:f:w:W:D:r:R:d:t:l:u:c:p:")) != -1) {
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
	        case 'r':
                res = oper_coda_ins(q, READLIST, optarg);
	            break;
	        case 'R':
                if (optarg != NULL) {

                    printf("optarg: %s\n",optarg);

                    if (optarg[0] != '-') res = oper_coda_ins(q, READN, optarg);
                    else {

                        optarg = argv[optind--];
                        res = oper_coda_ins(q, READN, "n=0");
                    }

                } else res = oper_coda_ins(q, READN, "n=0");
	            break;
	        case 'd':
                res = oper_coda_setRDirname(q, optarg);   //controlla se tail lista c'è READNFILES o READLIST,...se no errore
	            break;
	        case 't':
                time_ms = atoi(optarg);
                if (time_ms == 0) {

                    fprintf(stderr, "%d: -t number required\n", pid);
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

        if (res != 0) {                   //gestione errore

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

        fprintf(stderr, "%d: openConnection %s, ERRORE %s\n", pid, sockname, strerror(errno));
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
