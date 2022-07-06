#ifndef COMMON_FUNCS_H
#define COMMON_FUNCS_H

#include <sys/types.h>

// Legge "n" bytes dal descrittore

ssize_t readn(int fd, void *ptr, size_t n);

// Scrive "n" bytes per il descrittore

ssize_t writen(int fd, void *ptr, size_t n);

// Ricava il nome del file da 'path'

char *getFileName(char *path);

// Creo (ricavando il nome da 'pathname') e scrivo il file di contenuto 'buf' in 'dirname'

int createWriteInDir (char *pathname, void *buf, size_t size, char *dirname);

// Legge dal socket 'fd' 'nfiles' files e li scrive in dirname(se != NULL)

int writeSocketFiles(int fd, int nfiles, char *dirname);

// Controlla pathname

int checkPathname(const char* pathname);



#endif
