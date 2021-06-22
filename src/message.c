#include "message.h"
#include "utils.h"
#include <string.h>

/*
	Ritorna 0 in caso di successo della richiesta,
	-1 altrimenti.
*/
int sendMessageHeader(int socket_fd, opt_keys option, char *filename, int flags){
	char buffer[PATH_LEN_MAX];
	if( filename != NULL ){
		strncpy(buffer, filename, PATH_LEN_MAX);
	}
	else{
		memset(buffer, '\0', PATH_LEN_MAX);
	}
	if( writen(socket_fd, (void*)&option, sizeof(opt_keys)) == -1) return -1;
	if( writen(socket_fd, (void*)buffer, sizeof(char)*PATH_LEN_MAX) == -1) return -1;
	if( writen(socket_fd, (void*)&flags, sizeof(int)) == -1) return -1;
	return 0;
}

/*
	Si mette in ascolto sul file descriptor specificato, aspettando
	l'arrivo dell'header di un messaggio.
	Salva il contenuto dell'header in hdr, e restituisce -1 
	in caso di errore, 0 in caso di EOF ed un numero positivo in 
	caso di terminazione con successo.
*/
int receiveMessageHeader(int socket_fd, message_header_t *hdr){
	int res;
	res = readn(socket_fd, (void*)&hdr->option, sizeof(opt_keys));
	if( res <= 0 ) return res;
	res = readn(socket_fd, (void*)hdr->filename, sizeof(char)*PATH_LEN_MAX);
	if( res <= 0 ) return res;
	res = readn(socket_fd, (void*)&hdr->flags, sizeof(int));
	if( res <= 0 ) return res;
	return 1;
}

/*
	Ritorna 0 in caso di successo della richiesta,
	-1 altrimenti.
*/
int sendMessageContent(int socket_fd, size_t size, char* content){
	if( writen(socket_fd, (void*)&size, sizeof(size_t)) == -1) return -1;
	if( writen(socket_fd, (void*)content, size) == -1) return -1;
	return 0;
}

/*
	Si mette in ascolto sul file descriptor specificato, aspettando
	l'arrivo del content di un messaggio.
	Salva il contenuto del content in cnt, e restituisce -1 
	in caso di errore, 0 in caso di EOF e 1 in caso di successo.
*/
int receiveMessageContent(int socket_fd, message_content_t *cnt){
	int res;
	res = readn(socket_fd, (void*)&cnt->size, sizeof(size_t));
	if( res <= 0 ) return res;
	if( ( cnt->content = malloc(cnt->size) ) == NULL ) return -1;
	res = readn(socket_fd, (void*)cnt->content, cnt->size);
	if( res <= 0 ) return res;
	return 1;
}
