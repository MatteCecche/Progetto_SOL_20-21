#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/un.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>


#include "../../includes/def_tipodato.h"
#include "../../includes/fun_descrit.h"
#include "../../includes/api.h"

extern int pid;     // processo id del client
extern bool p;      // bool per stampa info si/no

int csfd = -1;



int checkPathname(const char* pathname) {

    if (pathname == NULL || pathname[0] == '\0' ) {

        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------- //
// Legge dal socket 'fd' 'nfiles' files e li scrive in dirname(se != NULL) //
// ----------------------------------------------------------------------- //

int writeSocketFiles(int fd, int nfiles, char *dirname) {

    if (dirname != NULL && dirname[0] != '\0') {

        struct stat statbuf;

        if (stat(dirname, &statbuf) == 0) {

            if (!S_ISDIR(statbuf.st_mode)) {                                                                                        //controlla se e' una cartella

                fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE %s is not a directory\n\e[0m", pid, dirname);
                dirname = NULL;

            } else {                                                                                                                //dirname è una cartella
                if (p && nfiles > 0){
                  printf("\e[0;32mCLIENT %d: Salvo nella cartella %s, %d files\n\e[0m", pid, dirname, nfiles);
                }
            }
        } else {

            fprintf(stderr, "\e[0;32mCLIENT %d: \e[0;31mERRORE stat %s, ERRORE %s\n\e[0m", pid, dirname, strerror(errno));
            dirname = NULL;
        }
    }

    msg_risposta_t res;
    memset(&res, '\0', sizeof(msg_risposta_t));


    int i = 0;
    while (i < nfiles) {

        memset(&res, 0, sizeof(msg_risposta_t));

        if (readn(csfd, &res, sizeof(msg_risposta_t)) != sizeof(msg_risposta_t)) {

            return -1;
        }

        if (res.result != 0) {                                                  // ha sbagliato qualcosa il server

            errno = EAGAIN;

            return -1;
        }

        char *buffer = malloc(res.datalen);
        if(readn(csfd, buffer, res.datalen) != res.datalen) {

            free(buffer);

            return -1;
        }

        if (p) printf("\e[0;32mCLIENT %d: Nella cartella %s, e' salvato il file %s di len %d\n\e[0m", pid, dirname, res.pathname, res.datalen);
        fflush(stdout);

        if (dirname != NULL && dirname[0] != '\0') {                                                            //dirname è una cartella valida (controllato all'inizio della funzione)

            if (createWriteInDir(res.pathname, buffer, res.datalen, dirname) != 0) {                            // creare e scrivere file nella cartella

                free(buffer);                                                                                   //errno dovrebbe settarlo creareWriteInDi

                return -1;
            }
        }

        free(buffer);
        i++;
    }

    return 0;
}



int openConnection(const char* sockname, int msec, const struct timespec abstime) {


    if (sockname == NULL) {

        errno = EINVAL;

        return -1;
    }

    struct sockaddr_un sa;
    strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
    sa.sun_family = AF_UNIX;

    csfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if(csfd < 0) {

        return -1;
    }

    int microseconds = msec *1000;
    struct timespec current_time;

    do {

        if (connect(csfd, (struct sockaddr*)&sa, sizeof(sa)) != -1) return 0;

        if (errno == ENOENT) usleep(microseconds);
        else {

            return -1;
        }

        if(clock_gettime(CLOCK_REALTIME, &current_time) == -1){

            close(csfd);

            return -1;
        }
    } while (current_time.tv_sec < abstime.tv_sec || current_time.tv_nsec < abstime.tv_nsec);

    errno =  ETIMEDOUT;

    return -1;
}



int closeConnection(const char* sockname) {

    int temp = csfd;
    csfd = -1;

    return close(temp);
}



int openFile(const char* pathname, int flags, const char* dirname) {

    if (checkPathname(pathname) != 0) {

        errno = EINVAL;

        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {

        return -1;
    }

    msg_richiesta_t *req = malloc(sizeof(msg_richiesta_t));
    msg_risposta_t *res = malloc(sizeof(msg_risposta_t));

    memset(req, 0, sizeof(msg_richiesta_t));
    memset(res, 0, sizeof(msg_risposta_t));

    req->op = OPENFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = flags;
    req->datalen = 0;                                                             //superfluo

    if(writen(csfd, req, sizeof(msg_richiesta_t)) != sizeof(msg_richiesta_t)) {

        free(req);
        free(res);

        return -1;
    }

    if(readn(csfd, res, sizeof(msg_risposta_t)) != sizeof(msg_risposta_t)) {

        free(req);
        free(res);

        return -1;
    }

    if(res->result != 0) {

        errno = res->result;
        free(res);
        free(req);

        return -1;
    }

    int nfiles = res->datalen;                                                                            //numero di files eliminati
    if (nfiles > 1 || (nfiles != 0 && (req->flag != O_CREATE_LOCK && req->flag != O_CREATE))) {

        free(res);
        free(req);
        errno = EAGAIN;                                                                                   //ha sbagliato qualcosa il server

        return -1;
    }

    free(req);

    free(res);

    return writeSocketFiles(csfd, nfiles, dirname);

}



int readFile(const char* pathname, void** buf, size_t* size) {

    if (checkPathname(pathname) != 0) {

        errno = EINVAL;

        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {

        return -1;
    }

    msg_richiesta_t *req = malloc(sizeof(msg_richiesta_t));
    msg_risposta_t *res = malloc(sizeof(msg_risposta_t));

    memset(req, 0, sizeof(msg_richiesta_t));
    memset(res, 0, sizeof(msg_risposta_t));

    req->op = READFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL;                                                   //superfluo
    req->datalen = 0;                                                     //superfluo

    if (writen(csfd, req, sizeof(msg_richiesta_t)) != sizeof(msg_richiesta_t)) {

        free(req);
        free(res);

        return -1;
    }

    free(req);

    if (readn(csfd, res, sizeof(msg_risposta_t)) != sizeof(msg_risposta_t)) {

        free(res);

        return -1;
    }
    int result = res->result;

    if(result == 0) {                                                     //esito positivo, mi verrà inviato il file

        char *buffer = malloc(res->datalen);
        if(readn(csfd, buffer, res->datalen) != res->datalen) {

            free(res);
            free(buffer);

            return -1;
        }

        *buf = buffer;
        *size = res->datalen;

        free(res);

        return 0;
    }
    else {

        free(res);
        errno = result;

        return -1;
    }

}



int readNFiles(int N, const char* dirname) {

    msg_richiesta_t *req = malloc(sizeof(msg_richiesta_t));
    msg_risposta_t *res = malloc(sizeof(msg_risposta_t));

    memset(req, 0, sizeof(msg_richiesta_t));
    memset(res, 0, sizeof(msg_risposta_t));

    req->op = READNFILES;
    req->flag = O_NULL;                                           //superfluo
    req->datalen = N;                                             // numero di files che richiediamo di leggere (datalen ha un significato diverso in altre operazioni)


    if (writen(csfd, req, sizeof(msg_richiesta_t)) != sizeof(msg_richiesta_t)) {

        free(req);
        free(res);

        return -1;
    }

    free(req);

    if (readn(csfd, res, sizeof(msg_risposta_t)) != sizeof(msg_risposta_t)) {

        free(res);

        return -1;
    }

    if(res->result != 0) {

        errno = res->result;
        free(res);

        return -1;
    }

    int nfiles = res->datalen;                                          //numero di files letti

    free(res);

    int esitowrites = writeSocketFiles(csfd, nfiles, dirname);
    if (esitowrites != -1) {

        return nfiles;

    } else return -1;

}



int writeFile(const char* pathname, const char* dirname) {

    if (checkPathname(pathname) != 0) {

        errno = EINVAL;

        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {

        return -1;
    }

    FILE *file;
    void *file_out = NULL;
    unsigned long file_size = 0;

    if ((file = fopen(abspathname,"r")) == NULL) {

        return -1;
    }

    if (file != NULL) {

        fseek(file, 0L, SEEK_END);              // Scopriamo la dimensione del file
        file_size = ftell(file);                // restituisce la posizione corrente del file pointer rispetto all’inizio del file
        rewind(file);

        file_out = (void *)malloc(file_size);
        if (file_out == NULL) {

            fclose(file);

            return -1;
        } else {

            size_t bytes_read = fread(file_out, 1, file_size, file);
            if (bytes_read != file_size){

                fclose(file);
                free(file_out);
                errno = EIO;

                return -1;
            }
        }

        fclose(file);
    }


    msg_richiesta_t *req = malloc(sizeof(msg_richiesta_t));
    msg_risposta_t *res = malloc(sizeof(msg_risposta_t));

    memset(req, 0, sizeof(msg_richiesta_t));
    memset(res, 0, sizeof(msg_risposta_t));

    req->op = WRITEFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL;                                             //superfluo
    req->datalen = file_size;

    if (writen(csfd, req, sizeof(msg_richiesta_t)) != sizeof(msg_richiesta_t)) {

        free(file_out);
        free(req);
        free(res);

        return -1;
    }

    free(req);

    if (writen(csfd, file_out, file_size) != file_size) {

        free(file_out);
        free(res);

        return -1;
    }

    free(file_out);

    if (readn(csfd, res, sizeof(msg_risposta_t)) != sizeof(msg_risposta_t)) {

        free(res);

        return -1;
    }

    if(res->result != 0) {

        errno = res->result;
        free(res);

        return -1;
    }

    int nfiles = res->datalen;                                    //numero di files eliminati

    free(res);

    return writeSocketFiles(csfd, nfiles, dirname);

}



int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {

    if (checkPathname(pathname) != 0) {

        errno = EINVAL;

        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {

        return -1;
    }

    msg_richiesta_t *req = malloc(sizeof(msg_richiesta_t));
    msg_risposta_t *res = malloc(sizeof(msg_risposta_t));

    memset(req, 0, sizeof(msg_richiesta_t));
    memset(res, 0, sizeof(msg_risposta_t));

    req->op = APPENDTOFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL;                                                 //superfluo
    req->datalen = size;

    if (writen(csfd, req, sizeof(msg_richiesta_t)) != sizeof(msg_richiesta_t)) {

        free(req);
        free(res);

        return -1;
    }

    free(req);

    if (writen(csfd, buf, size) != size) {

        free(res);

        return -1;
    }

    if (readn(csfd, res, sizeof(msg_risposta_t)) != sizeof(msg_risposta_t)) {

        free(res);

        return -1;
    }

    if(res->result != 0) {

        errno = res->result;
        free(res);

        return -1;
    }

    int nfiles = res->datalen;                                    //numero di files eliminati

    free(res);

    return writeSocketFiles(csfd, nfiles, dirname);

}



int lockFile(const char* pathname) {

    if (checkPathname(pathname) != 0) {

        errno = EINVAL;

        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {

        return -1;
    }

    msg_richiesta_t *req = malloc(sizeof(msg_richiesta_t));
    msg_risposta_t *res = malloc(sizeof(msg_risposta_t));

    memset(req, 0, sizeof(msg_richiesta_t));
    memset(res, 0, sizeof(msg_risposta_t));

    req->op = LOCKFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL;                                         //superfluo
    req->datalen = 0;                                           //superfluo

    if (writen(csfd, req, sizeof(msg_richiesta_t)) != sizeof(msg_richiesta_t)) {

        free(req);
        free(res);

        return -1;
    }

    free(req);


    if (read(csfd, res, sizeof(msg_risposta_t)) != sizeof(msg_risposta_t)) {      // mi blocco sulla read, fino a quando il server non mi dà il lock, o errore per qualche motivo

        free(res);

        return -1;
    }
    int result = res->result;

    free(res);

    if(result != 0) {

        errno = result;

        return -1;
    }

    return 0;
}



int unlockFile(const char* pathname) {

    if (checkPathname(pathname) != 0) {

        errno = EINVAL;

        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {

        return -1;
    }

    msg_richiesta_t *req = malloc(sizeof(msg_richiesta_t));
    msg_risposta_t *res = malloc(sizeof(msg_risposta_t));

    memset(req, 0, sizeof(msg_richiesta_t));
    memset(res, 0, sizeof(msg_risposta_t));

    req->op = UNLOCKFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL;                                                         //superfluo
    req->datalen = 0;                                                           //superfluo

    if (writen(csfd, req, sizeof(msg_richiesta_t)) != sizeof(msg_richiesta_t)) {

        free(req);
        free(res);

        return -1;
    }

    free(req);

    if (readn(csfd, res, sizeof(msg_risposta_t)) != sizeof(msg_risposta_t)) {

        free(res);

        return -1;
    }
    int result = res->result;

    free(res);

    if(result != 0) {

        errno = result;

        return -1;
    }

    return 0;
}



int closeFile(const char* pathname) {

    if (checkPathname(pathname) != 0) {

        errno = EINVAL;

        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {

        return -1;
    }

    msg_richiesta_t *req = malloc(sizeof(msg_richiesta_t));
    msg_risposta_t *res = malloc(sizeof(msg_risposta_t));

    memset(req, 0, sizeof(msg_richiesta_t));
    memset(res, 0, sizeof(msg_risposta_t));

    req->op = CLOSEFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL;                                                         //superfluo
    req->datalen = 0;                                                           //superfluo

    if (writen(csfd, req, sizeof(msg_richiesta_t)) != sizeof(msg_richiesta_t)) {

        free(req);
        free(res);

        return -1;
    }

    free(req);

    if (readn(csfd, res, sizeof(msg_risposta_t)) != sizeof(msg_risposta_t)) {

        free(res);

        return -1;
    }
    int result = res->result;

    free(res);

    if(result != 0) {

        errno = result;

        return -1;
    }

    return 0;
}



int removeFile(const char* pathname) {

    if (checkPathname(pathname) != 0) {

        errno = EINVAL;

        return -1;
    }

    char abspathname[PATH_MAX];
    if (!realpath(pathname, abspathname)) {

        return -1;
    }

    msg_richiesta_t *req = malloc(sizeof(msg_richiesta_t));
    msg_risposta_t *res = malloc(sizeof(msg_risposta_t));

    memset(req, 0, sizeof(msg_richiesta_t));
    memset(res, 0, sizeof(msg_risposta_t));

    req->op = REMOVEFILE;
    strncpy(req->pathname, abspathname, strlen(abspathname)+1);
    req->flag = O_NULL;                                                         //superfluo
    req->datalen = 0;                                                           //superfluo

    if (writen(csfd, req, sizeof(msg_richiesta_t)) != sizeof(msg_richiesta_t)) {

        free(req);
        free(res);

        return -1;
    }

    free(req);

    if (readn(csfd, res, sizeof(msg_risposta_t)) != sizeof(msg_risposta_t)) {

        free(res);

        return -1;
    }
    int result = res->result;

    free(res);

    if (result != 0) {

        errno = result;

        return -1;
    }

    return 0;
}
