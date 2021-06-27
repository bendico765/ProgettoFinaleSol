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
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>

static char saved_sockname[UNIX_PATH_MAX]; // nome del socket
static int saved_fd = -1; // fd del socket

/* File di utilità usate dalle implementazioni delle api */

/*
	Legge il contenuto del file identificato dal pathname
	e restituisce un puntatore all'area di memoria 
	contenente il contenuto del file, salvando in size
	la dimensione (in bytes) del file.
	
	Restituisce NULL in caso di errore
*/
void* getFileContent(const char* pathname, size_t *size){
	FILE *file;
	struct stat statbuf;
	size_t file_size;
	char *content;
	int read_chars;
	
	if( (file = fopen(pathname, "rb")) != NULL ){
		// ottenimento dimensione file
		if( stat(pathname, &statbuf) == -1){
			fclose(file);
			return NULL;
		}
		// allocazione memoria per il contenuto
		file_size = (size_t) statbuf.st_size;
		if( (content = malloc(file_size)) == NULL ){
			fclose(file);
			return NULL;
		}
		read_chars = fread((void*)content, file_size, 1, file);
		content[file_size/sizeof(char)-1] = '\0';
		if( read_chars == -1 ){
			free(content);
			fclose(file);
			return NULL;
		}
		fclose(file);
		*size = file_size;
		return (void*)content;
	}
	return NULL;
}

/*
	La funzione gestisce la ricezione di num_files files mandati 
	dal server (identificato da fd) in seguito allo sforamento
	dei limiti della cache, e li salva nella cartella dirname.
	
	Se dirname è NULL, i files sono buttati via.
	
	Restituisce 0 in caso di successo, -1 altrimenti.
*/
int writeReceivedFilesToDir(int fd, const char* dirname, int num_files){
	message_t *recv_message;

	// scrivo ogni file espulso nella eventuale directory
	for(int i = 0; i < num_files; i++){
		
		// ricevo il messaggio con il file
		recv_message = receiveMessage(saved_fd);
		if( recv_message == NULL ) return -1;
		
		// scrivo il file espulso nella directory
		if( dirname != NULL ){ 
			char *new_path;
			char *relative_path;
			FILE *file;
			size_t new_path_size;
			
			// ricavo il nome del file
			relative_path = absolutePathToFilename(recv_message->hdr->filename);
			
			// ricavo la grandezza del nuovo path
			new_path_size = strlen(dirname) + strlen(relative_path) + 2;
			// creo il path in cui scrivere il file
			if( (new_path = malloc(new_path_size*sizeof(char))) == NULL ) return -1;
			memset(new_path, '\0', new_path_size);
			strcat(new_path, dirname);
			strcat(new_path, "/");
			strcat(new_path, relative_path);
			
			// apro il file
			file = fopen(new_path, "wb");
			if( file == NULL ) return -1;
			if( fwrite((const void*)recv_message->cnt->content, recv_message->cnt->size, 1, file) == -1) return -1;
			
			fclose(file);
			free(relative_path);
			free(new_path);
		}
		// libero il contenuto del file
		freeMessage(recv_message);
	}
	return 0;
}

/**************/

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
	int socket_fd;
	struct sockaddr_un sa;
	
	if( msec >= 1000 ){ // massimo valore in msec consentito
		errno = EINVAL;
		return -1;
	}
	if( saved_fd != -1 ){ // esiste già una connessione aperta
		errno = EADDRINUSE;
		return -1;
	}	
	
	if( (socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 ) return -1;
	strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
	strncpy(saved_sockname, sockname, UNIX_PATH_MAX);
	sa.sun_family = AF_UNIX;
	if( connect(socket_fd, (struct sockaddr*)&sa, sizeof(sa)) != 0){
		// faccio i vari tentativi
		int conn_result = -1;
		struct timespec real_time;
		struct timespec first_delay = {0, msec*1000000};
		
		if( clock_gettime(CLOCK_REALTIME, &real_time) == -1 ) return -1;
		
		// continuo ad attendere fino alla connessione o fino allo 
		// scadere del tempo assoluto
		while( real_time.tv_sec < abstime.tv_sec && ( conn_result = connect(socket_fd, (struct sockaddr*)&sa, sizeof(sa))) != 0){
			fprintf(stderr, "Tentativo di connessione\n");
			// attendo il tempo del ritardo
			if( nanosleep(&first_delay, NULL) == -1 ) return -1;
			// prendo l'ora corrente
			if( clock_gettime(CLOCK_REALTIME, &real_time) == -1 ) return -1;
		}
		if( conn_result == -1 ){
			errno = ECONNREFUSED;
			return -1;
		}
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
	int res;
	message_t *recv_message;
	// mando la richiesta
	if( sendMessage(saved_fd, OPEN_FILE_OPT, pathname, flags, 0, NULL) == -1 ) return -1;
	// ottengo la risposta
	if( (recv_message = receiveMessage(saved_fd)) == NULL ) return -1;
	switch( recv_message->hdr->option ){
		case SUCCESS:
			errno = 0;
			res = 0;
			break;
		case FILE_ALREADY_EXIST:
			errno = EEXIST;
			res = -1;
			break;
		case INEXISTENT_FILE:
			errno = ENOENT;
			res = -1;
			break;
		case INVALID_FLAG:
			errno = EINVAL;
			res = -1;
			break;
		default:
			break;
	}
	freeMessage(recv_message);
	return res;
}

/*
	Rimuove il file cancellandolo dal file storage server. L’operazione 
	fallisce se il file non è in stato locked, o è in stato locked da 
	parte di un processo client diverso da chi effettua la removeFile.
*/
int removeFile(const char* pathname){
	int res;
	message_t *recv_message;
	// mando la richiesta
	if( sendMessage(saved_fd, REMOVE_FILE_OPT, pathname, 0, 0, NULL) == -1 ) return -1;
	// ottengo la risposta
	recv_message = receiveMessage(saved_fd);
	if( recv_message == NULL ) return -1;
	switch( recv_message->hdr->option ){
		case SUCCESS:
			errno = 0;
			res = 0;
			break;
		case INEXISTENT_FILE:
			errno = ENOENT;
			res = -1;
			break;
		default:
			break;
	}
	freeMessage(recv_message);
	return res;
}

/*
	Scrive tutto il file puntato da pathname nel file server. 
	Ritorna successo solo se la precedente operazione, terminata 
	con successo, è stata openFile(pathname, O_CREATE| O_LOCK). 
	Se ‘dirname’ è diverso da NULL, il file eventualmente spedito 
	dal server perchè espulso dalla cache per far posto al file 
	‘pathname’ dovrà essere scritto in ‘dirname’; 
	Ritorna 0 in caso di successo, -1 in caso di fallimento, 
	errno viene settato opportunamente.
*/
int writeFile(const char* pathname, const char* dirname){ 
	message_t *recv_message;
	size_t size;
	int res;
	DIR *dir = NULL;
	
	// verifica la validità di pathname
	if( pathname == NULL ){
		errno = EINVAL;
		return -1;
	}
	
	// verifica l'esistenza della eventuale directory
	if( dirname != NULL && (dir = opendir(dirname)) == NULL ){
		errno = ENOENT;
		return -1;
	}
	
	// richiedo di scrivere su file
	if( sendMessage(saved_fd, WRITE_FILE_OPT, pathname, 0, 0, NULL) == -1 ) return -1;
	
	// controllo di aver ottenuto il permesso
	recv_message = receiveMessage(saved_fd);
	if( recv_message == NULL ) return -1;
	if( recv_message->hdr->option == SUCCESS ){ // permesso accordato
		void *content;
		freeMessage(recv_message);
		// apro il file
		if( ( content = getFileContent(pathname, &size)) == NULL ) return -1;
		// spedisco il file
		if( sendMessage(saved_fd, WRITE_FILE_OPT, pathname, 0, size, (char*) content) == -1 ) return -1;
		// ricevo l'esito dell'operazione
		recv_message = receiveMessage(saved_fd);
		if( recv_message == NULL ) return -1;
		switch( recv_message->hdr->option ){
			case SUCCESS: // scrittura avvenuta con successo
				errno = 0;
				res = 0;
				break;
			case CACHE_SUBSTITUTION: // file espulsi dallo storage
				errno = 0;
				res = writeReceivedFilesToDir(saved_fd, dirname, recv_message->hdr->flags);
				break;
			case FILE_TOO_BIG: // file troppo grande per la cache
				errno = EFBIG;
				res = -1;
				break;
			default:
				break;
		}
		free(content);
	}
	else{
		if( recv_message->hdr->option == INEXISTENT_FILE ){
			errno = ENOENT;
		}
		else{ // la precedente operazione non era una openFile
			errno = EPERM;
		}
		res = -1;
	}
	if( dir != NULL ) free(dir);
	freeMessage(recv_message);
	return res;
}

/*
	Legge tutto il contenuto del file dal server (se esiste) 
	ritornando un puntatore ad un'area allocata sullo heap nel
	parametro ‘buf’, mentre ‘size’ conterrà la dimensione del buffer 
	dati (ossia la dimensione in bytes del file letto). 
	In caso di errore, ‘buf‘e ‘size’ non sono validi. 
	Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene
	settato opportunamente.
*/
int readFile(const char* pathname, void** buf, size_t* size){
	message_t *msg;
	size_t temp_size;
	int res;
	void *copy;
	
	// mando il messaggio richiedendo il file
	if( sendMessage(saved_fd, READ_FILE_OPT, pathname, 0, 0, NULL) == -1 ) return -1;
	
	// ricevo la risposta del server
	msg = receiveMessage(saved_fd);
	if( msg == NULL ) return -1;
	switch( msg->hdr->option ){
		case SUCCESS:
			// salvo il contenuto del file
			temp_size = msg->cnt->size;
			if( ( copy = malloc(temp_size) ) == NULL ) return -1;
			memcpy(copy, msg->cnt->content, temp_size);
			*buf = copy;
			*size =	temp_size;	
			errno = 0;
			res = 0;
			break;
		case INEXISTENT_FILE:
			errno = ENOENT;
			res = -1;
			break;
		default:
			break;
	}
	freeMessage(msg);
	return res;	
}

/*
	Richiesta di scrivere in append al file ‘pathname‘ i ‘size‘ bytes 
	contenuti nel buffer ‘buf’. L’operazione di append nel file è 
	garantita essere atomica dal file server. 
	Se ‘dirname’ è diverso da NULL, il file eventualmente spedito dal 
	server perchè espulso dalla cache per far posto ai nuovi dati di 
	‘pathname’ dovrà essere scritto in ‘dirname’;
	Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene 
	settato opportunamente.
*/
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname){
	int res;
	message_t *recv_message;
	DIR *dir = NULL;
	
	// controllo validità di pathname e buffer
	if( pathname == NULL || buf == NULL ){
		errno = EINVAL;
		return -1;
	}
	// controllo l'esistenza dell'eventuale dirname
	if( dirname != NULL && (dir = opendir(dirname)) == NULL ){
		errno = ENOENT;
		return -1;
	}
	
	// controllo l'esistenza del file
	if( sendMessage(saved_fd, APPEND_TO_FILE_OPT, pathname, 0, 0, NULL) == -1 ) return -1;
	
	recv_message = receiveMessage(saved_fd);
	if( recv_message == NULL ) return -1;
	if( recv_message->hdr->option == SUCCESS ){ // permesso di append accordato
		freeMessage(recv_message);
		// mando le modifiche da appendere
		if( sendMessage(saved_fd, APPEND_TO_FILE_OPT, pathname, 0, size, buf) == -1 ) return -1;
		// ricevo l'esito dell'operazione
		recv_message = receiveMessage(saved_fd);
		if( recv_message == NULL ) return -1;
		switch( recv_message->hdr->option ){
			case SUCCESS: // append avvenuto con successo
				errno = 0;
				res = 0;
				break;
			case CACHE_SUBSTITUTION: // file espulsi dallo storage
				errno = 0;
				res = writeReceivedFilesToDir(saved_fd, dirname, recv_message->hdr->flags);
				break;
			case FILE_TOO_BIG:
				errno = EFBIG;
				res = -1;
				break;
			default:
				break;
		}
	}
	else{ // file non esistente
		errno = ENOENT;
		res = -1;
	}
	freeMessage(recv_message);
	free(dir);
	return res;
}

/*
	Richiede al server la lettura di ‘N’ files qualsiasi 
	da memorizzare nella directory ‘dirname’ lato client. 
	Se il server ha meno di ‘N’ file disponibili, li invia tutti. 
	Se N<=0 la richiesta al server è quella di leggere tutti i 
	file memorizzati al suo interno. 
	Ritorna un valore maggiore o uguale a 0 in caso di successo 
	(cioè ritorna il n. di file effettivamente letti), -1 in caso 
	di fallimento, errno viene settato opportunamente.
*/
int readNFiles(int N, const char* dirname){
	int num_files;
	DIR *dir;
	message_t *recv_message;
	
	// controllo l'esistenza dell'eventuale dirname
	dir = NULL;
	if( dirname != NULL && (dir = opendir(dirname)) == NULL ){
		errno = ENOENT;
		return -1;
	}
	
	// mando la richiesta di lettura
	if( sendMessage(saved_fd, READ_N_FILE_OPT, NULL, N, 0, NULL) == -1 ) return -1;
	recv_message = receiveMessage(saved_fd);
	if( recv_message == NULL ) return -1;

	// scrivo il numero di files ricevuti nella directory
	num_files = recv_message->hdr->flags;
	if( writeReceivedFilesToDir(saved_fd, dirname, num_files) == -1 ) return -1;
	
	freeMessage(recv_message);
	if(dir != NULL) free(dir);
	return num_files;
}



/*
	Richiesta di chiusura del file puntato da ‘pathname’. 
	Eventuali operazioni sul file dopo la closeFile falliscono.
	Ritorna 0 in caso di successo, -1 in caso di fallimento, 
	errno viene settato opportunamente.
*/
int closeFile(const char* pathname){
	int res = 0;
	message_t *recv_message;
	
	// controllo validità di pathname
	if( pathname == NULL ){
		errno = EINVAL;
		return -1;
	}
	// mando la richiesta di chisura
	if( sendMessage(saved_fd, CLOSE_FILE_OPT, pathname, 0, 0, NULL) == -1 ) return -1;
	// ottengo la risposta dal server
	recv_message = receiveMessage(saved_fd);
	if( recv_message == NULL ) return -1;
	switch( recv_message->hdr->option ){
		case SUCCESS:
			errno = 0;
			res = 0;
			break;
		case INEXISTENT_FILE:
			errno = ENOENT;
			res = -1;
			break;
		default:
			break;
	}
	freeMessage(recv_message);
	return res;
}

/*
	Chiude la connessione AF_UNIX associata al socket 
	file sockname. 
	Ritorna 0 in caso di successo, -1 in caso di
	fallimento, errno viene settato opportunamente.
*/
int closeConnection(const char* sockname){
	// controllo validità di pathname
	if( sockname == NULL ){
		errno = EINVAL;
		return -1;
	}
	// controllo che corrisponda al socket name aperto
	if( saved_fd == -1 || strncmp(saved_sockname, sockname, UNIX_PATH_MAX) != 0 ){
		errno = EBADF;
		return -1;
	}
	if( close(saved_fd) == -1 ) return -1;
	saved_fd = -1;
	return 0;
}
