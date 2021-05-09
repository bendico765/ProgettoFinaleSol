#if !defined(_UTILS_H)
#define _UTILS_H

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#define ce_null(var,msg) if( (var) == NULL ){perror(msg); exit(-1);}
#define ce_less1(var,msg) if( (var) == -1 ){perror(msg); exit(-1);}

int isNumber(const char* s, long* n);

/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
ssize_t readn(int fd, void *ptr, size_t n);
 
/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
ssize_t writen(int fd, void *ptr, size_t n);

#endif /* _UTILS_H */
