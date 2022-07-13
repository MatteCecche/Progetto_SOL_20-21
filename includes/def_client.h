#ifndef DEF_CLIENT_H
#define DEF_CLIENT_H

typedef enum {

    WRITEDIRNAME = 1,   // -w
    WRITELIST = 2,      // -W
    APPEND = 3,         // -a
    READLIST = 4,       // -r
    READN = 5,          // -R
    LOCKLIST = 6,       // -l
    UNLOCKLIST = 7,     // -u
    REMOVELIST = 8      // -r

} input_opt;

#endif
