#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#include "../../includes/fun_descrit.h"


ssize_t readn(int fd, void *ptr, size_t n) {

    size_t   nleft;
    ssize_t  nread;

    nleft = n;
    while (nleft > 0) {

        if((nread = read(fd, ptr, nleft)) < 0) {      //legge da fd numero ptr bytes immagazzinandoli nel buffer puntato da nleft.

            if (nleft == n) return -1;                // errore
            else break;                               // torna quello letto fino ad ora
        } else if (nread == 0) break;
        nleft -= nread;
        ptr   += nread;
    }
    return(n - nleft);                                // ritorna un n >= 0
}


ssize_t writen(int fd, void *ptr, size_t n) {

    size_t   nleft;
    ssize_t  nwritten;

    nleft = n;
    while (nleft > 0) {
        if((nwritten = write(fd, ptr, nleft)) < 0) {   // scrive da fd numero ptr bytes immagazzinandoli nel buffer puntato da nleft.
            if (nleft == n) return -1;                 // errore
            else break;                                // torna quello scritto fino ad ora
        } else if (nwritten == 0) break;
        nleft -= nwritten;
        ptr   += nwritten;
    }

    return(n - nleft); // ritorna n >= 0
}

// --------------------------------- //
// Ricava il nome del file da 'path' //
// --------------------------------- //

char *getFileName(char *path){

    char *filename = strrchr(path, '\/'); // \\ su windows
    if (filename == NULL)

        filename = path;
    else
        filename++;

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
    if (fd == -1) {

        return -1;
    }

    if (write(fd, buf, size) == -1) {

        return -1;
    }

    if (close(fd) == -1) {

        return -1;
    }

    return 0;

}
