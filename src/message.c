#include "message.h"
#include "utils.h"
#include <string.h>

/*
	Funzione per mandare l'header di un messaggio al file
	descriptor socket_fd;

	Ritorna 0 in caso di successo della richiesta,
	-1 altrimenti.
*/
int sendMessageHeader(int socket_fd, opt_keys option, const char *filename, int flags){
	char buffer[PATH_LEN_MAX];
	if( filename != NULL ){
		strncpy(buffer, filename, PATH_LEN_MAX);
	}
	else{
		memset(buffer, '\0', PATH_LEN_MAX * sizeof(char));
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
	Funzione per mandare il content di un messaggio al file
	descriptor socket_fd;

	Ritorna 0 in caso di successo della richiesta,
	-1 altrimenti.
*/
int sendMessageContent(int socket_fd, size_t size, void* content){
	if( writen(socket_fd, (void*)&size, sizeof(size_t)) == -1) return -1;
	if( writen(socket_fd, (void*)content, size) == -1) return -1;
	return 0;
}

/*
	Si mette in ascolto sul file descriptor specificato, aspettando
	l'arrivo del content di un messaggio.
	Il contenuto del messaggio è allocato dinamicamente.
	
	Salva il contenuto del content in cnt, e restituisce -1 
	in caso di errore, 0 in caso di EOF e 1 in caso di successo.
*/
int receiveMessageContent(int socket_fd, message_content_t *cnt){
	int res;
	res = readn(socket_fd, (void*)&cnt->size, sizeof(size_t));
	if( res <= 0 ) return res;
	if( cnt->size != 0 ){
		if( ( cnt->content = malloc(cnt->size) ) == NULL ) return -1;
		res = readn(socket_fd, (void*)cnt->content, cnt->size);
		if( res <= 0 ) return res;
	}
	else{
		cnt->content = NULL;
	}
	
	return 1;
}

/*
	Manda il messaggio con i parametri specificati al socket_fd 
	restituendo 0 in caso di successo o -1 in caso di errore 
*/
int sendMessage(int socket_fd, opt_keys option, const char *filename, int flags, size_t size, void* content){
	if( sendMessageHeader(socket_fd, option, filename, flags) == -1 ) return -1;
	if( sendMessageContent(socket_fd, size, content) == -1 ) return -1;
	return 0;
}
/*
	Restituisce il messaggio ricevuto da socket_fd, o 
	restituisce NULL in caso di errore
*/
message_t* receiveMessage(int socket_fd){
	message_t *msg;
	message_header_t *hdr;
	message_content_t *cnt;
	
	if( ( msg = malloc(sizeof(message_t)) ) == NULL ) return NULL;
	if( ( hdr = malloc(sizeof(message_header_t)) ) == NULL ) return NULL;
	if( ( cnt = malloc(sizeof(message_content_t)) ) == NULL ) return NULL;
	
	// inizializzazione valori
	hdr->option = 0;
	memset((void*)hdr->filename, '\0', PATH_LEN_MAX);
	hdr->flags = 0;
	cnt->content = NULL;
	cnt->size = 0;
	

	// ricezione header
	if( receiveMessageHeader(socket_fd, hdr) <= 0 ){
		free(hdr);
		free(cnt);
		free(msg);
		return NULL;
	}
	
	// ricezione contenuto
	if( receiveMessageContent(socket_fd, cnt) <= 0 ){
		free(hdr);
		if( cnt->content != NULL ) free(cnt->content);
		free(cnt);
		free(msg);
		return NULL;
	}
	
	msg->hdr = hdr;
	msg->cnt = cnt;
	
	return msg;
}

void freeMessage(message_t *msg){
	if( msg != NULL ){
		free(msg->hdr);
		if( msg->cnt->content != NULL ) free(msg->cnt->content);
		free(msg->cnt);
		free(msg);
	}
}

