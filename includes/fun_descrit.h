#ifndef FUN_DESCRIT_H
#define FUN_DESCRIT_H

#include <sys/types.h>


// -------------------------------- //
// Legge "n" bytes                  //
// -------------------------------- //

ssize_t readn(int fd, void *ptr, size_t n);


// -------------------------------- //
// Scrive "n" bytes                 //
// -------------------------------- //

ssize_t writen(int fd, void *ptr, size_t n);


// ---------------------------------------------------------------------------------------- //
// Crea (ricavando il nome da 'pathname') e scrive il file di contenuto 'buf' in 'dirname'  //
// ---------------------------------------------------------------------------------------- //

int createWriteInDir (char *pathname, void *buf, size_t size, char *dirname);


#endif
