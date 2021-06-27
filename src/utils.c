#include "utils.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

int isIntNumber(const char* s, int* n) {
	if (s==NULL) 
		return 1;
	if ( strlen(s) == 0) 
		return 1;
	char* e = NULL;
	errno = 0;
	long val = strtol(s, &e, 10);
	if (errno == ERANGE) 
		return 2; // overflow
	if (e != NULL && *e == (char)0) {
		if( val <= INT_MAX ){
			*n = val;
			return 0;   // successo
		}
		else{
			return 2; // overflow
		}
	}
	return 1;   // non e' un numero
}

int isNumber(const char* s, long* n) {
	if (s==NULL) 
		return 1;
	if ( strlen(s) == 0) 
		return 1;
	char* e = NULL;
	errno = 0;
	long val = strtol(s, &e, 10);
	if (errno == ERANGE) 
		return 2; // overflow
	if (e != NULL && *e == (char)0) {
		*n = val;
		return 0;   // successo
		}
	return 1;   // non e' un numero
}

/** Evita letture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la lettura da fd leggo EOF
 *   \retval size se termina con successo
 */
ssize_t readn(int fd, void *ptr, size_t n) {  
   size_t   nleft;
   ssize_t  nread;
 
   nleft = n; 
   while (nleft > 0) {
     if( (nread = read(fd, ptr, nleft)) < 0) {
        if (nleft == n) 
        	return -1; /* error, return -1 */
        else 
        	break; /* error, return amount read so far */
     } 
     else 
     	if (nread == 0) 
     		break; /* EOF */
     nleft -= nread;
     ptr   += nread;
   }
   return(n - nleft); /* return >= 0 */
}
 
/** Evita scritture parziali
 *
 *   \retval -1   errore (errno settato)
 *   \retval  0   se durante la scrittura la write ritorna 0
 *   \retval  1   se la scrittura termina con successo
 */
ssize_t writen(int fd, void *ptr, size_t n) {  
   size_t   nleft;
   ssize_t  nwritten;
 
   nleft = n;
   while (nleft > 0) {
     if( (nwritten = write(fd, ptr, nleft)) < 0) {
        if (nleft == n) 
        	return -1; /* error, return -1 */
        else 
        	break; /* error, return amount written so far */
     } 
     else 
     	if (nwritten == 0) 
     		break; 
     nleft -= nwritten;
     ptr   += nwritten;
   }
   return(n - nleft); /* return >= 0 */
}

/*
	Dato un path assoluto, estrapola il nome del file riferito
*/
char *absolutePathToFilename(char *absolute_path){
	char *token;
	char *old_token;
	char *copy;
	char *res;
	if( (copy = strdup(absolute_path)) == NULL) return NULL;
	token = strtok(copy, "/");
	while( token != NULL ){
		old_token = token;
		token = strtok(NULL, "/");
	}
	res = strdup(old_token);
	free(copy);
	return res;
}

/*
	Dato il percorso relativo ad un file, prova a risolvere il percorso
	assoluto e lo restituisce in caso di successo, o restituisce NULL
	in caso di errore (errno settato). La memoria per il path assoluto 
	è allocata dinamicamente e deve essere successivamente liberata.
*/
char *relativeToAbsolutePath(char *relative_path){
	return realpath(relative_path, NULL);
}
