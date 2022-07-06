#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#include "common_funcs.h"


ssize_t readn(int fd, void *ptr, size_t n) {

    size_t   nleft;     //byte rimasti
    ssize_t  nread;     //byte letti

    nleft = n;
    while (nleft > 0) {

        if((nread = read(fd, ptr, nleft)) < 0) {

            if (nleft == n) return -1;    //errore
            else break;                   // errore, ritorna bytes letti fino a quel momento

        } else if (nread == 0) break;     //fine

        nleft -= nread;
        ptr   += nread;
    }

    return(n - nleft);                    // ritorna un numero >= 0

}


ssize_t writen(int fd, void *ptr, size_t n) {

    size_t   nleft;           //byte rimasti
    ssize_t  nwritten;        //byte scritti

    nleft = n;
    while (nleft > 0) {

        if((nwritten = write(fd, ptr, nleft)) < 0) {

            if (nleft == n) return -1;        //errore
            else break;                       // errore, ritorna bytes scritti fino a quel momento

        } else if (nwritten == 0) break;      //fine

        nleft -= nwritten;
        ptr   += nwritten;
    }

    return(n - nleft);                        // ritorna un numero >= 0

}


char *getFileName(char *path){

    char *filename = strrchr(path, '\/'); // \\ su windows

    if (filename == NULL) filename = path;
    else filename++;

    return filename;
}


int createWriteInDir (char *pathname, void *buf, size_t size, char *dirname) {

    char *filename = getFileName(pathname);
    int len = strlen(filename) + strlen(dirname) + 2;
    char *final_pathname = malloc(len);
    if (final_pathname == NULL) return -1;

    strcpy(final_pathname, dirname);
    strcat(final_pathname,"/");
    strcat(final_pathname, filename);

    int fd = open(final_pathname, O_WRONLY|O_CREAT|O_TRUNC, 0666); // file descriptor

    free(final_pathname);

    if (fd == -1) return -1;

    if (write(fd, buf, size) == -1) return -1;

    if (close(fd) == -1) return -1;

    return 0;
}


int writeSocketFiles(int fd, int nfiles, char *dirname) {

    if (dirname != NULL && dirname[0] != '\0') {

        struct stat statbuf;
        if (stat(dirname, &statbuf) == 0) {

            if (!S_ISDIR(statbuf.st_mode)) {     //errore, non e' una directory

                fprintf(stderr, "%d: %s is not a directory\n", pid, dirname);
                dirname = NULL;
            } else if (p && nfiles > 0) printf("%d: Salvo in dirname %s, %d files\n", pid, dirname, nfiles);        //E' una directory
          } else {

            fprintf(stderr, "%d: stat %s, ERRORE %s\n", pid, dirname, strerror(errno));
            dirname = NULL;
        }
    }

    msg_response_t res;
    memset(&res, '\0', sizeof(msg_response_t));


    int i = 0;
    while (i < nfiles) {

        memset(&res, 0, sizeof(msg_response_t));

        if (readn(csfd, &res, sizeof(msg_response_t)) != sizeof(msg_response_t)) return -1;

        if (res.result != 0) {

            errno = EAGAIN;       // errore server
            return -1;
        }

        char *buffer = malloc(res.datalen);

        if(readn(csfd, buffer, res.datalen) != res.datalen) {

            free(buffer);
            return -1;
        }

        if (p) printf("%d: Salva in dirname %s, il file %s di len %d\n", pid, dirname, res.pathname, res.datalen);
        fflush(stdout);

        if (dirname != NULL && dirname[0] != '\0') {              //dirname è una directory valida (controllato all'inizio della funzione)

            // creare e scrivere file nella directory
            if (createWriteInDir(res.pathname, buffer, res.datalen, dirname) != 0) {

                //errno dovrebbe settarlo creareWriteInDir
                free(buffer);
                return -1;
            }
        }

        free(buffer);
        i++;
    }

    return 0;
}


int checkPathname(const char* pathname) {

    if (pathname == NULL || pathname[0] == '\0' ) return -1;

    return 0;

}
