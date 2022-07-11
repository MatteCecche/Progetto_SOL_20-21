#ifndef COMMON_FUNCS_H
#define COMMON_FUNCS_H

#include <sys/types.h>

// legge "n" bytes dal destrittore

ssize_t readn(int fd, void *ptr, size_t n);

// Scrive "n" bytes sul descittore

ssize_t writen(int fd, void *ptr, size_t n);

// Crea (ricavando il nome da 'pathname') e scrive il file di contenuto 'buf' in 'dirname'

int createWriteInDir (char *pathname, void *buf, size_t size, char *dirname);


#endif
