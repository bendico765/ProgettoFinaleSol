#include "api.h"
#include "utils.h"
#include "message.h"
#include "opt_keys.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <stdio.h>

int nanosleep(const struct timespec *req, struct timespec *rem);

char saved_sockname[UNIX_PATH_MAX];
int saved_fd = -1;

/*
	Viene aperta una connessione AF_UNIX al socket file sockname.
	Se il server non accetta immediatamente la richiesta di
	connessione, la connessione da parte del client viene 
	ripetuta dopo ‘msec’ millisecondi e fino allo
	scadere del tempo assoluto ‘abstime’ specificato come terzo 
	argomento. 
	Ritorna 0 in caso di successo, -1 in caso di fallimento, 
	errno viene settato opportunamente.
*/
int openConnection(const char* sockname, int msec, const struct timespec abstime){
	if( msec >= 1000 ){ // massimo valore in msec consentito
		errno = EINTR;
		return -1;
	}
	if( saved_fd != -1 ){ // esiste già una connessione aperta
		errno = EADDRINUSE;
		return -1;
	}
	int socket_fd, connect_result;
	struct sockaddr_un sa;
	if( (socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 ) return -1;
	strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
	strncpy(saved_sockname, sockname, UNIX_PATH_MAX);
	sa.sun_family = AF_UNIX;
	if( (connect_result = connect(socket_fd, (struct sockaddr*)&sa, sizeof(sa))) != 0){
		// faccio i vari tentativi
		struct timespec first_delay = {0, msec*1000000};
		while( (connect_result = connect(socket_fd, (struct sockaddr*)&sa, sizeof(sa))) != 0 ){
			fprintf(stderr, "Provo a connettermi");
			nanosleep(&first_delay, NULL);
		}
		// AGGIUNGERE LA GESTIONE DEL TIMER PER ABSTIME
	}
	saved_fd = socket_fd;
	return 0;
}

/*
	Richiesta di apertura o di creazione di un file. 
	La semantica della openFile dipende dai flags passati come 
	secondo argomento che possono essere O_CREATE ed O_LOCK. 
	Se viene passato il flag O_CREATE ed il file esiste già
	memorizzato nel server, oppure il file non esiste ed il 
	flag O_CREATE non è stato specificato, viene ritornato un
	errore. In caso di successo, il file viene sempre aperto in 
	lettura e scrittura, ed in particolare le scritture possono
	avvenire solo in append. Se viene passato il flag O_LOCK 
	(eventualmente in OR con O_CREATE) il file viene
	aperto e/o creato in modalità locked, che vuol dire che 
	l’unico che può leggere o scrivere il file ‘pathname’ è il
	processo che lo ha aperto. 
	Il flag O_LOCK può essere esplicitamente resettato 
	utilizzando la chiamata unlockFile, descritta di seguito.
	Ritorna 0 in caso di successo, -1 in caso di fallimento, 
	errno viene settato opportunamente.
*/
int openFile(const char* pathname, int flags){
	message_header_t *recv_message = malloc(sizeof(message_header_t));
	if( recv_message == NULL ) return -1;
	if( sendMessageHeader(saved_fd, OPEN_FILE_OPT, pathname, flags) == -1 ) return -1;
	if( receiveMessageHeader(saved_fd, recv_message) == -1 ) return -1;
	if( recv_message->option == SUCCESS ){
		free(recv_message);
		return 0;
	}
	free(recv_message);
	return -1;
}

/*
	Chiude la connessione AF_UNIX associata al socket 
	file sockname. 
	Ritorna 0 in caso di successo, -1 in caso di
	fallimento, errno viene settato opportunamente.
*/
int closeConnection(const char* sockname){
	if( strncmp(saved_sockname, sockname, UNIX_PATH_MAX) != 0 ){
		return -1;
	}
	if( close(saved_fd) == -1 ){
		return -1;
	}
	saved_fd = -1;
	return 0;
}
