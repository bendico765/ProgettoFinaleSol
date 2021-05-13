#include "socket_utils.h"
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

/*
	La funzione prova a creare il socket di nome
	sockname; ritorna il file descriptor della socket
	se la creazione è avvenuta con successo, altrimenti 
	imposta errno e ritorna -1
*/
int initializeServerAndStart(char *sockname){
	int fd;
	struct sockaddr_un sa;
	if( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 ) return -1;
	strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
	sa.sun_family = AF_UNIX;
	if( bind(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1 ) return -1;
	if( listen(fd, SOMAXCONN) == -1 ) return -1;
	return fd;
}

/*
	La funzione prova a creare una connessione con il  
	socket di nome sockname; ritorna il file descriptor 
	con cui comunicare col socket se la creazione è 
	avvenuta con successo, altrimenti imposta errno e 
	ritorna -1
*/
int initializeClientAndConnect(char *sockname){
	int socket_fd;
	struct sockaddr_un sa;
	if( (socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 ) return -1;
	strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
	sa.sun_family = AF_UNIX;
	while(connect(socket_fd, (struct sockaddr*)&sa, sizeof(sa)) == -1){
		if( errno == ENOENT ){
			sleep(1);
		}
		else{
			return -1;
		}
	}
	return socket_fd;
}

