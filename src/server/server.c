#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/un.h>
#include <time.h>

#include "utils.h"
#include "queue.h"
#include "stats.h"
#include "thread_utils.h"
#include "config_parser.h"
#include "signal_handler.h"
#include "message.h"
#include "storage.h"
#include "file.h"

#define LOG_MSG_LEN 512

// Mutex e cond. var. della coda tra dispatcher e workers
pthread_mutex_t fd_queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fd_queue_cond = PTHREAD_COND_INITIALIZER;

// Mutex della struct delle statistiche
pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

// Mutex dello storage
pthread_mutex_t storage_lock = PTHREAD_MUTEX_INITIALIZER;

// Mutex per la scrittura sul file di log
pthread_mutex_t logfile_lock = PTHREAD_MUTEX_INITIALIZER;

// argomenti da passare ai thread workers
typedef struct{
	storage_t *storage; // storage dei files
	queue_t *fd_queue; // coda dei fd da servire
	int fd_pipe; // pipe per restituire i fd serviti al dispatcher
	FILE *logfile; // file di log
	stats_t *server_stats; // struct delle statistiche
} thread_arg_t;

void updateStats(storage_t *storage, stats_t *server_stats, int num_expelled_files){
	if( storage != NULL && server_stats != NULL ){
		server_stats->num_files = storageGetNumElements(storage);
		if( server_stats->num_files > server_stats->max_num_files ) server_stats->max_num_files = server_stats->num_files;
		server_stats->dim_storage = storageGetSizeElements(storage);
		if( server_stats->dim_storage > server_stats->max_dim_storage ) server_stats->max_dim_storage = server_stats->dim_storage;
		if( num_expelled_files > 0 ) server_stats->cache_substitutions += 1;	
	}
}

/*
	Restituisce 0 in caso di successo, -1 in caso di errore
*/
int openFileHandler(int fd, message_t *msg, FILE *logfile, storage_t *storage, stats_t *server_stats){
	int res;
	file_t *file;
	char opt_string[LOG_MSG_LEN];
	
	snprintf(opt_string, LOG_MSG_LEN, "THREAD [%ld] : openFile(%s,%d)", pthread_self(), msg->hdr->filename, msg->hdr->flags);
	res = 0;
	errno = 0;
	
	// controllo la validità del flag O_CREATE
	if( msg->hdr->flags != 0 && msg->hdr->flags != 1 ){
		ce_less1(lock(&logfile_lock), "Errore lock logfile");
		fprintf(logfile, "%s : Flag non valido\n", opt_string);
		ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
		
		return sendMessage(fd, INVALID_FLAG, NULL, 0, 0, NULL);
	}
	// controllo se O_CREATE = 0 ed il file non esiste
	
	ce_less1( lock(&storage_lock), "Errore lock storage openFile" );
	// cerco il file nello storage
	file = (file_t*) storageFind(storage, msg->hdr->filename);

	if( msg->hdr->flags == 0 ){
		if( file == NULL ){
			ce_less1(lock(&logfile_lock), "Errore lock logfile");
			fprintf(logfile, "%s : File inesistente\n", opt_string);
			ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
			
			res = sendMessage(fd, INEXISTENT_FILE, NULL, 0, 0, NULL);
		}
		else{
			// imposto che il file è stato appena aperto
			file->is_open = 1;
			res = sendMessage(fd, SUCCESS, NULL, 0, 0, NULL);
		}
	}
	else{
		// controllo se O_CREATE = 1 ed il file esiste già
		if( file != NULL ){
			ce_less1(lock(&logfile_lock), "Errore lock logfile");
			fprintf(logfile, "%s : File già esistente\n", opt_string);
			ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
			
			res = sendMessage(fd, FILE_ALREADY_EXIST, NULL, 0, 0, NULL);
		}
		else{ // creo il file e lo apro
			file_t *new_file;
			queue_t *expelled_files;
			int num_expelled_files;
			
			new_file = fileGenerate(msg->hdr->filename, NULL, 0);
			ce_null(new_file, "Errore generazione nuovo file");
			
			// inserisco il file nello storage
			errno = 0;
			expelled_files = storageInsert(storage, (void*)new_file->pathname, (void*)new_file);
			if( errno != 0 ){
				perror("Errore nell'inserimento in cache durante openFile");
				exit(-1);
			}
			
			// dealloco gli eventuali files espulsi
			num_expelled_files = queueLen(expelled_files);
			
			ce_less1(lock(&logfile_lock), "Errore lock logfile");
			if( num_expelled_files > 0 ){
				fprintf(logfile, "%s : Completata con l'espulsione di %d files dallo storage\n", opt_string, num_expelled_files);	
				fprintf(logfile, "Files espulsi:\n");
				queuePrint(expelled_files, filePrintInfo, logfile);	
			}
			else{
				fprintf(logfile, "%s : Terminata con successo\n", opt_string);	
			}
			ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
			
			queueDestroy(expelled_files, fileFree);
			
			// aggiorno le statistiche
			ce_less1( lock(&stats_lock), "Errore lock statistiche openFile" );
			updateStats(storage, server_stats, num_expelled_files);
			ce_less1( unlock(&stats_lock), "Errore unlock statistiche openFile" );

			res = sendMessage(fd, SUCCESS, NULL, 0, 0, NULL);
		}
	}
	ce_less1( unlock(&storage_lock), "Errore unlock storage openFile" );
	return res;
}

int closeFileHandler(int fd, message_t *msg, FILE *logfile, storage_t *storage){
	int res;
	file_t *file;
	char opt_string[LOG_MSG_LEN];
	
	snprintf(opt_string, LOG_MSG_LEN, "THREAD [%ld] : closeFile(%s)", pthread_self(), msg->hdr->filename);
	res = 0;
	errno = 0;
	
	ce_less1( lock(&storage_lock), "Errore lock storage closeFile" );
	// cerco il file nello storage
	file = (file_t*) storageFind(storage, msg->hdr->filename);
	if( file == NULL ){
		if( errno == ENOENT ){ // file inesistente
			ce_less1(lock(&logfile_lock), "Errore lock logfile");
			fprintf(logfile, "%s : File inesistente\n", opt_string);
			ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
			
			res = sendMessage(fd, INEXISTENT_FILE, NULL, 0, 0, NULL);
		}
		else{ // errori gravi riguardanti la memoria nello heap	
			ce_less1(lock(&logfile_lock), "Errore lock logfile");
			fprintf(logfile, "%s : Errore durante la chiusura del file\n", opt_string);
			ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
			
			exit(-1);
		}
	}
	else{ // file trovato
		file->freshly_opened = 0;
		file->is_open = 0;
		
		ce_less1(lock(&logfile_lock), "Errore lock logfile");
		fprintf(logfile, "%s : Terminata con successo\n", opt_string);
		ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
		
		res = sendMessage(fd, SUCCESS, NULL, 0, 0, NULL);
	}
	ce_less1( unlock(&storage_lock), "Errore unlock storage closeFile" );
	return res;
}

int removeFileHandler(int fd, message_t *msg, FILE *logfile, storage_t *storage, stats_t *server_stats){
	int res;
	char opt_string[LOG_MSG_LEN];
	
	snprintf(opt_string, LOG_MSG_LEN, "THREAD [%ld] : removeFile(%s)", pthread_self(), msg->hdr->filename);
	res = 0;
	errno = 0;
	
	ce_less1( lock(&storage_lock), "Errore lock storage removeFile" );
	
	// tento la rimozione del file
	if( storageRemove(storage, (void*)msg->hdr->filename, NULL, fileFree) == -1 ){
		if( errno == ENOENT ){ // file non trovato
			ce_less1(lock(&logfile_lock), "Errore lock logfile");
			fprintf(logfile, "%s : File inesistente\n", opt_string);
			ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
			
			res = sendMessage(fd, INEXISTENT_FILE, NULL, 0, 0, NULL);
		}
		else{ // errori gravi riguardanti la memoria nello heap	
			ce_less1(lock(&logfile_lock), "Errore lock logfile");
			fprintf(logfile, "%s : Errore durante la rimozione del file\n", opt_string);
			ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
					
			exit(-1);
		}
	}
	else{ // rimozione avvenuta con successo
		ce_less1(lock(&logfile_lock), "Errore lock logfile");
		fprintf(logfile, "%s : Terminata con successo\n", opt_string);
		ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
	
		// aggiorno le statistiche
		ce_less1( lock(&stats_lock), "Errore lock statistiche removeFile" );
		updateStats(storage, server_stats, 0);
		ce_less1( unlock(&stats_lock), "Errore unlock statistiche removeFile" );
		
		res = sendMessage(fd, SUCCESS, NULL, 0, 0, NULL);
	}
	
	ce_less1( unlock(&storage_lock), "Errore unlock storage removeFile" );
	
	return res;
}

int writeFileHandler(int fd, message_t *msg, FILE *logfile, storage_t *storage, stats_t *server_stats){
	int res;
	file_t *file;
	char opt_string[LOG_MSG_LEN];
	
	snprintf(opt_string, LOG_MSG_LEN, "THREAD [%ld] : writeFile(%s)", pthread_self(), msg->hdr->filename);
	res = 0;
	errno = 0;
	
	ce_less1( lock(&storage_lock), "Errore lock storage closeFile" );
	
	// cerco il file nello storage
	file = (file_t*) storageFind(storage, msg->hdr->filename);
	if( file == NULL ){
		if( errno == ENOENT ){
			ce_less1(lock(&logfile_lock), "Errore lock logfile");
			fprintf(logfile, "%s : File inesistente\n", opt_string);
			ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
			
			res = sendMessage(fd, INEXISTENT_FILE, NULL, 0, 0, NULL);
		}
		else{ // errori gravi riguardanti la memoria nello heap	
			ce_less1(lock(&logfile_lock), "Errore lock logfile");
			fprintf(logfile, "%s : Errore durante la scrittura del del file\n", opt_string);
			ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
					
			exit(-1);
		}
	}
	else{
		// controllo se la precedente operazione era una openFile(pathname, O_CREATE);
		if( file->freshly_opened != 1 ){ 
			ce_less1(lock(&logfile_lock), "Errore lock logfile");
			fprintf(logfile, "%s : File non aperto\n", opt_string);
			ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
			
			res = sendMessage(fd, FILE_NOT_OPENED, NULL, 0, 0, NULL);
		}
		else{
			int num_expelled_files;
			queue_t *expelled_files;
			message_t *file_msg;
			
			// segnalo al client di mandare il contenuto del file
			res = sendMessage(fd, SUCCESS, NULL, 0, 0, NULL);
			if( res != 0 ){ 
				ce_less1( unlock(&storage_lock), "Errore unlock storage writeFile" );
				return -1;
			}
			
			// ricevo il contenuto del messaggio
			file_msg = receiveMessage(fd);
			if( file_msg == NULL ){
				ce_less1( unlock(&storage_lock), "Errore unlock storage writeFile" );
				return -1;
			}
			
			// segno che il file è stato aperto
			file->freshly_opened = 0;
			
			// espello eventuali file dalla cache
			errno = 0;
			expelled_files = storageEditElem(storage, (void*)msg->hdr->filename, file_msg->cnt->content, file_msg->cnt->size, fileEdit, fileAreDifferent);
			
			if( expelled_files == NULL ){
				switch(errno){
					case EFBIG: // file troppo grande per la cache
						ce_less1(lock(&logfile_lock), "Errore lock logfile");
						fprintf(logfile, "%s : File troppo grande (%ld bytes) per lo storage\n", opt_string, file_msg->cnt->size);
						ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
						
						res = sendMessage(fd, FILE_TOO_BIG, NULL, 0, 0, NULL);
						break;
					default: //errori gravi
						perror("Errore storageEditElem writeFile");
						exit(-1);
				}
			}
			else{ // modifica avvenuta con successo
				num_expelled_files = queueLen(expelled_files);
				
				// aggiorno le statistiche
				ce_less1( lock(&stats_lock), "Errore lock statistiche writeFile" );
				updateStats(storage, server_stats, num_expelled_files);	
				ce_less1( unlock(&stats_lock), "Errore unlock statistiche writeFile" );
				
				if( num_expelled_files > 0 ){ // mando i files espulsi al client
					file_t *expelled_file;
					
					ce_less1(lock(&logfile_lock), "Errore lock logfile");
					fprintf(logfile, "%s : Terminata con successo (%ld bytes scritti) ed espulsione di %d files dallo storage\n", opt_string, file_msg->cnt->size, num_expelled_files);
					fprintf(logfile, "Files espulsi:\n");
					queuePrint(expelled_files, filePrintInfo, logfile);	
					ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
					
					res = sendMessage(fd, CACHE_SUBSTITUTION, NULL, num_expelled_files, 0, NULL);
					if( res != 0 ){
						freeMessage(file_msg);
						queueDestroy(expelled_files, fileFree);
						ce_less1( unlock(&storage_lock), "Errore unlock storage writeFile" );
						return res;
					}
					
					// mando i files espulsi al client
					while( (expelled_file = (file_t*) queueRemove(expelled_files)) != NULL ){
						res = sendMessage(fd, CACHE_SUBSTITUTION, expelled_file->pathname, 0, expelled_file->size, expelled_file->content);
						fileFree(expelled_file);
						if( res == -1 ){
							freeMessage(file_msg);
							queueDestroy(expelled_files, fileFree);
							ce_less1( unlock(&storage_lock), "Errore unlock storage writeFile" );
							return res;
						}
					}				
				}
				else{
					ce_less1(lock(&logfile_lock), "Errore lock logfile");
					fprintf(logfile, "%s : Terminata con successo (%ld bytes scritti)\n", opt_string, file_msg->cnt->size);
					ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
					
					res = sendMessage(fd, SUCCESS, NULL, 0, 0, NULL);
				}
			}
			
			freeMessage(file_msg);
			queueDestroy(expelled_files, fileFree);
		}
	}
	ce_less1( unlock(&storage_lock), "Errore unlock storage writeFile" );
	return res;
}

int readFileHandler(int fd, message_t *msg, FILE *logfile, storage_t *storage){
	int res;
	file_t *file;
	char opt_string[LOG_MSG_LEN];
	
	snprintf(opt_string, LOG_MSG_LEN, "THREAD [%ld] : readFile(%s)", pthread_self(), msg->hdr->filename);
	res = 0;
	errno = 0;
		
	ce_less1( lock(&storage_lock), "Errore lock storage readFile" );
	
	// cerco il file nello storage
	file = (file_t*) storageFind(storage, msg->hdr->filename);
	if( file == NULL ){
		if( errno == ENOENT ){ // file non trovato
			ce_less1(lock(&logfile_lock), "Errore lock logfile");
			fprintf(logfile, "%s : File inesistente\n", opt_string);
			ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
			
			res = sendMessage(fd, INEXISTENT_FILE, NULL, 0, 0, NULL);
		}
		else{ // errori gravi riguardanti la memoria nello heap	
			ce_less1(lock(&logfile_lock), "Errore lock logfile");
			fprintf(logfile, "%s : Errore durante la lettura del file\n", opt_string);
			ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
			
			exit(-1);
		}
	}
	else{
		// segno che il file è stato letto
		file->freshly_opened = 0;
		
		ce_less1(lock(&logfile_lock), "Errore lock logfile");
		fprintf(logfile, "%s : Terminata con successo\n", opt_string);
		ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
		
		// mando il file al client
		res = sendMessage(fd, SUCCESS, file->pathname, 0, file->size, file->content);
	}
	ce_less1( unlock(&storage_lock), "Errore unlock storage readFile" );
	return res;
}

int readNFileHandler(int fd, message_t *msg, FILE *logfile, storage_t *storage){
	file_t *file;
	queue_t *shallow_copy_queue;
	int res;
	int num_requested_files;
	int num_files_sent;
	char opt_string[LOG_MSG_LEN];
	
	snprintf(opt_string, LOG_MSG_LEN, "THREAD [%ld] : readNFile(%d)", pthread_self(), msg->hdr->flags);
	res = 0;
	errno = 0;
	num_requested_files = msg->hdr->flags;
		
	ce_less1( lock(&storage_lock), "Errore lock storage readFile" );
	
	if( num_requested_files <= 0 ){
		num_requested_files = storageGetNumElements(storage);
	}
	
	// prelevo gli elementi richiesti dallo storage
	shallow_copy_queue = storageGetNElems(storage, num_requested_files);
	if( shallow_copy_queue == NULL ){
		ce_less1(lock(&logfile_lock), "Errore lock logfile");
		fprintf(logfile, "%s : Errore durante la lettura dei files\n", opt_string);
		ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
		
		exit(-1);
	}
	
	num_files_sent = queueLen(shallow_copy_queue);
	
	ce_less1(lock(&logfile_lock), "Errore lock logfile");
	fprintf(logfile, "%s : Invio di %d files\n", opt_string, num_files_sent);
	queuePrint(shallow_copy_queue, filePrintInfo, logfile);
	ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
	
	// mando il numero di files disponibili
	res = sendMessage(fd, READ_N_FILE_OPT, NULL, num_files_sent, 0, NULL);
	if( res == 0 ){ // se non ci sono stati errori, mando i files
		while( (file = queueRemove(shallow_copy_queue)) != NULL){
			res = sendMessage(fd, READ_N_FILE_OPT, file->pathname, 0, file->size, file->content);
			if( res != 0 ) break;
		}
	}

	ce_less1( unlock(&storage_lock), "Errore unlock storage readFile" );
	
	queueDestroy(shallow_copy_queue, NULL);
	return res;
}

int appendToFileHandler(int fd, message_t *msg, FILE *logfile, storage_t *storage, stats_t *server_stats){
	int res;
	file_t *file;
	char opt_string[LOG_MSG_LEN];
	
	snprintf(opt_string, LOG_MSG_LEN, "THREAD [%ld] : appendToFile(%s)", pthread_self(), msg->hdr->filename);
	res = 0;
	errno = 0;
	
	ce_less1( lock(&storage_lock), "Errore lock storage appendToFile" );
	
	// cerco il file nello storage
	file = (file_t*) storageFind(storage, msg->hdr->filename);
	if( file == NULL ){
		ce_less1(lock(&logfile_lock), "Errore lock logfile");
		fprintf(logfile, "%s : File inesistente\n", opt_string);
		ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
		
		res = sendMessage(fd, INEXISTENT_FILE, NULL, 0, 0, NULL);
	}
	else{
		int num_expelled_files; // numero files espulsi dalla cache
		queue_t *expelled_files; // coda files espulsi dalla cache
		message_t *append_msg; // messaggio contenete le modifiche da appendere
		void *new_content; // nuovo contenuto del file
		size_t new_size; // nuova dimensone del file
		
		// segnalo al client di mandare la modifica
		res = sendMessage(fd, SUCCESS, NULL, 0, 0, NULL);
		if( res != 0 ){
			ce_less1( unlock(&storage_lock), "Errore unlock storage appendToFile" );
			return -1;
		}
		
		// ricevo le modifiche da apportare
		append_msg = receiveMessage(fd);
		if( append_msg == NULL ){
			ce_less1( unlock(&storage_lock), "Errore unlock storage appendToFile" );
			return -1;
		}
		
		// segno che il file è stato modificato
		file->freshly_opened = 0;
		// faccio l'operazione di append
		new_size = file->size + append_msg->cnt->size;
		ce_null(new_content = malloc(new_size), "Errore malloc");
		memcpy(new_content, file->content, file->size);
		memcpy((void*)((unsigned char *)(new_content) + file->size), append_msg->cnt->content, append_msg->cnt->size);
		// espello eventuali file dalla cache
		errno = 0;
		expelled_files = storageEditElem(storage, (void*)msg->hdr->filename, new_content, new_size, fileEdit, fileAreDifferent);
		
		if( expelled_files == NULL ){
			switch( errno ){
				case EFBIG: // file troppo grande per la cache
					ce_less1(lock(&logfile_lock), "Errore lock logfile");
					fprintf(logfile, "%s : La modifica è troppo grande per entrare nello storage\n", opt_string);
					ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
					
					res = sendMessage(fd, FILE_TOO_BIG, NULL, 0, 0, NULL);
					break;
				default:
					perror("Errore appendToFile");
					exit(-1);
			}
		}
		else{ // modifica avvenuta con successo
			num_expelled_files = queueLen(expelled_files);
			
			// aggiorno le statistiche
			ce_less1( lock(&stats_lock), "Errore lock statistiche appendToFile" );
			updateStats(storage, server_stats, num_expelled_files);	
			ce_less1( unlock(&stats_lock), "Errore unlock statistiche appendToFile" );
			
			if( num_expelled_files > 0 ){ // mando i files espulsi al client
				file_t *expelled_file;
				
				ce_less1(lock(&logfile_lock), "Errore lock logfile");
				fprintf(logfile, "%s : Terminata con successo, ed espulsione di %d files dallo storage\n", opt_string, num_expelled_files);
				fprintf(logfile, "Files espulsi:\n");
				queuePrint(expelled_files, filePrintInfo, logfile);	
				ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
				
				res = sendMessage(fd, CACHE_SUBSTITUTION, NULL, num_expelled_files, 0, NULL);
				
				if( res != 0 ){
					free(new_content);
					freeMessage(append_msg);
					queueDestroy(expelled_files, fileFree);
					ce_less1( unlock(&storage_lock), "Errore unlock storage appendToFile" );
					return -1;
				}
				
				// se non ci sono stati problemi, mando i files espulsi al client
				while( (expelled_file = (file_t*) queueRemove(expelled_files)) != NULL){
					res = sendMessage(fd, CACHE_SUBSTITUTION, expelled_file->pathname, 0, expelled_file->size, expelled_file->content);
					if( res != 0 ){
						free(new_content);
						freeMessage(append_msg);
						queueDestroy(expelled_files, fileFree);
						ce_less1( unlock(&storage_lock), "Errore unlock storage appendToFile" );
						return -1;
					}
					fileFree(expelled_file);
				}
			}
			else{
				ce_less1(lock(&logfile_lock), "Errore lock logfile");
				fprintf(logfile, "%s : Terminata con successo\n", opt_string);
				ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
				
				res = sendMessage(fd, SUCCESS, NULL, 0, 0, NULL);
			}
		}
		
		free(new_content);
		freeMessage(append_msg);
		queueDestroy(expelled_files, fileFree);
	}
	ce_less1( unlock(&storage_lock), "Errore unlock storage appendToFile" );
	return res;
}

/*
	Funzione eseguita da un thread worker
*/
void* workerThread(void* arg){
	storage_t *storage = ((thread_arg_t*)arg)->storage;
	queue_t *fd_queue = ((thread_arg_t*)arg)->fd_queue;
	int fd_pipe =  ((thread_arg_t*)arg)->fd_pipe;
	FILE* logfile = ((thread_arg_t*)arg)->logfile;
	stats_t *server_stats = ((thread_arg_t*)arg)->server_stats;
	int termination_flag;
	
	termination_flag = 0;
	while( termination_flag == 0){
		int fd; // fd prelevato dalla coda
		void *tmp;
		
		// Sezione critica: prelevo il fd da servire dalla coda
		ce_less1( lock(&fd_queue_lock), "Fallimento della lock in un thread worker");
		while( (tmp = queueRemove(fd_queue)) == NULL ){ // coda vuota: aspetto
			ce_less1( cond_wait(&fd_queue_cond, &fd_queue_lock), "Fallimento della wait in un thread worker");
		}
		ce_less1( unlock(&fd_queue_lock), "Fallimento della unlock in un thread worker");
		
		fd = *((int*)(tmp));
		free(tmp);
		
		if( fd == -1 ){ // segnale di terminazione per il worker
			termination_flag = 1;
		}
		else{ // ricevo l'header del messaggio
			int res;
			message_t *request = receiveMessage(fd);
			if( request != NULL ){ // messaggio arrivato correttamente
				// servo il file descriptor
				switch( request->hdr->option ){
					case OPEN_FILE_OPT:
						res = openFileHandler(fd, request, logfile, storage, server_stats);
						break;
					case CLOSE_FILE_OPT:
						res = closeFileHandler(fd, request, logfile, storage);
						break;
					case READ_FILE_OPT:
						res = readFileHandler(fd, request, logfile, storage);
						break;
					case READ_N_FILE_OPT:
						res = readNFileHandler(fd, request, logfile, storage);
						break;
					case WRITE_FILE_OPT:
						res = writeFileHandler(fd, request, logfile, storage, server_stats);
						break;
					case APPEND_TO_FILE_OPT:
						res = appendToFileHandler(fd, request, logfile, storage, server_stats);
						break;
					case REMOVE_FILE_OPT:
						res = removeFileHandler(fd, request, logfile, storage, server_stats);
						break;
					default:
						ce_less1(lock(&logfile_lock), "Errore lock logfile");
						fprintf(logfile, "THREAD [%ld]: opzione invalida\n", pthread_self());
						ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
						res = sendMessage(fd, INVALID_OPTION, NULL, 0, 0, NULL);
						break;
				
				}
			}
			if( request == NULL || res != 0 ){ // errore: disconnessione del client
				int tmp = -1;
				
				ce_less1(lock(&logfile_lock), "Errore lock logfile");
				fprintf(logfile, "THREAD [%ld]: chiudo la connessione con [%d]\n", pthread_self(), fd);
				ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
				
				// comunico la disconnessione di un client
				ce_less1( writen(fd_pipe, (void*)&tmp, sizeof(int)), "Fallimento scrittura pipe thread worker");
				
				// chiudo l'effettivo fd, rendendolo nuovamente disponibile al dispatcher
				close(fd);
			}
			else{ // scrivo il fd nella pipe, preparando il dispatcher a servirlo nuovamente
				ce_less1( writen(fd_pipe, (void*)&fd, sizeof(int)), "Fallimento scrittura pipe thread worker");
			}
			
			if( request != NULL ) freeMessage(request);
		}
	}
	return 0;
}

/*
	Funzione che termina e dealloca number_workers threads worker, mandando 
	il segnale di terminazione nella coda fd_queue
*/
void terminateWorkers(queue_t *fd_queue, pthread_t workers[], int number_workers){
	// invio del segnale di terminazione
	for(int i = 0; i < number_workers; i++){
		int *value = malloc(sizeof(int));
		ce_null(value, "Errore malloc terminateWorkers");
		*value = -1;
		
		ce_less1( lock(&fd_queue_lock), "Errore della lock nella terminazione dei workers");
		ce_null(queueInsert(fd_queue, (void*)value), "Errore queueInsert nella terminazione dei workers");
		ce_less1( cond_signal(&fd_queue_cond), "Errore nella signal della terminazione dei workers");
		ce_less1( unlock(&fd_queue_lock), "Errore nella unlock della terminazione dei workers");
	}
	// join dei thread
	for(int i = 0; i < number_workers; i++){
		errno = pthread_join(workers[i], NULL);
		if( errno != 0 ){
			perror("Errore nella pthread_join dei workers");
			return;
		}
	}
	free(workers);
}

/*
	Funzione che inizializza number_workers threads, passando arg_struct
	come argomento. 
	
	Restituise il puntatore al vettore di thread inizializzati, NULL
	in caso di errore
*/
pthread_t* initializeWorkers(int number_workers, thread_arg_t *arg_struct){
	pthread_t *workers = malloc(sizeof(pthread_t) * number_workers);
	if( workers != NULL ){
		for(int i = 0; i < number_workers; i++){
			if( (errno = pthread_create(&(workers[i]), NULL, workerThread, (void*)arg_struct)) != 0 )
				return NULL; 
		}
	}
	return workers;
}

void printStats(FILE *stream, stats_t *server_stats, storage_t *storage){
	ce_less1(lock(&logfile_lock), "Errore lock logfile");
	fprintf(stream, "---------\n");
	fprintf(stream, "NUMERO FILES MEMORIZZATI: %ld\n", server_stats->num_files);
	fprintf(stream, "DIMENSIONE FILE STORAGE (bytes): %ld\n", server_stats->dim_storage);
	fprintf(stream, "MASSIMO NUMERO DI FILES MEMORIZZATI DURANTE L'ESECUZIONE: %ld\n", server_stats->max_num_files);
	fprintf(stream, "MASSIMA DIMENSIONE DELLO STORAGE RAGGIUNTA DURANTE L'ESECUZIONE (bytes): %ld\n", server_stats->max_dim_storage);
	fprintf(stream, "NUMERO DI VOLTE IN CUI L'ALGORITMO DI CACHING È ENTRATO IN FUNZIONE: %d\n", server_stats->cache_substitutions);
	fprintf(stream, "FILES MEMORIZZATI A FINE ESECUZIONE:\n");
	storagePrint(storage, filePrintInfo, stream);
	fprintf(stream, "---------\n");
	ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
}

/*
	La funzione prova a creare il socket di nome
	sockname; ritorna il file descriptor della socket
	se la creazione è avvenuta con successo, altrimenti 
	imposta errno e ritorna -1
*/
int initializeServerAndStart(char *sockname, int max_connections){
	int fd;
	struct sockaddr_un sa;
	
	if( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 ) return -1;
	strncpy(sa.sun_path, sockname, UNIX_PATH_MAX);
	sa.sun_family = AF_UNIX;
	if( bind(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1 ) return -1;
	if( listen(fd, max_connections) == -1 ) return -1;
	
	return fd;
}

int main(int argc, char *argv[]){
	storage_t *storage; // storage dei files
	config_t server_config; // configurazione del server
	queue_t *fd_queue; // coda dispatcher/workers
	pthread_t *workers; // vettore di thread workers
	FILE *logfile; // file in cui stampare i messaggi di log 
	stats_t *server_stats; // struct contenente le statistiche
	fd_set set; // insieme fd attivi
	fd_set rdset; // insieme fd attesi in lettura
	int socket_fd; // fd del socket del server
	int fd_num; // max fd attivo
	int signal_handler_pipe[2]; // pipe main/signal handler
	int fd_pipe[2]; // pipe dispatcher/workers
	int connected_clients_counter; // contatore dei clients connessi
	int soft_termination_flag;
	int hard_termination_flag;

	// controllo correttezza argomenti
	if( argc != 2 ){ 
		fprintf(stderr, "%s file_config\n", argv[0]); 
		return -1;
	}
	
	// lettura dal file di config
	switch(parseConfigFile(argv[1], &server_config)){
		case -1:
			fprintf(stderr, "Impossibile aprire il file di configurazione\n");
			return -1;
		case -2:
			fprintf(stderr, "Errore durante il parsing delle opzioni\n");
			return -1;
		default:
			break;
	}
	
	// inizializzazione dello stream di log
	ce_null(logfile = fopen(server_config.log_filename, "a"), "Errore nell'apertura del file di log");
	
	// inizializzo la coda dispatcher/workers
	ce_null(fd_queue = queueCreate(), "Errore creazione coda dispatcher/workers");
	
	// inizializzo le pipes
	ce_less1(pipe(signal_handler_pipe), "Errore signal handler pipe");
	ce_less1(pipe(fd_pipe), "Errore pipe dispatcher/threads");
	
	// inizializzo il signal handler
	ce_less1(initializeSignalHandler(signal_handler_pipe), "Errore inizializzazione signal handler");
	
	// inizializzo lo storage
	storage = storageCreate(server_config.num_buckets_file, NULL, NULL, fileGetSize, fileGetPathname, fileEqual, server_config.max_memory, server_config.max_num_files, server_config.cache_type);
	ce_null(storage, "Errore nell'inizializzazione dello storage");
		
	// inizializzo la struct delle statistiche
	ce_null( server_stats = malloc(sizeof(stats_t)), "Errore creazione struct stats");
	memset((void*)server_stats, '\0', sizeof(stats_t));
	
	// inizializzo il pool di workers
	thread_arg_t arg_struct = {storage, fd_queue, fd_pipe[1], logfile, server_stats};
	ce_null(workers = initializeWorkers(server_config.thread_workers, &arg_struct), "Errore nell'inizializzazione dei thread");
	
	// inizializzo il server
	socket_fd = initializeServerAndStart(server_config.socket_name, server_config.max_connections);
	ce_less1(socket_fd, "Errore nell'inizializzazione della socket");
	
	ce_less1(lock(&logfile_lock), "Errore lock logfile");
	fprintf(logfile, "Server avviato\n");
	fprintf(logfile, "In attesa di connessioni...\n");
	ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
	
	// inizializzazione generazione numeri pseudocasuali (richiesto per gestire la readNFiles)
	srand((unsigned)time(NULL));
	
	// metto il massimo indice di descrittore 
	// attivo in fd_num
	fd_num = 0;
	if( socket_fd > fd_num ){
		fd_num = socket_fd;
	}
	if( signal_handler_pipe[0] > fd_num ){
		fd_num = signal_handler_pipe[0];
	}
	if( fd_pipe[0] > fd_num ){
		fd_num = fd_pipe[0];
	}
	FD_ZERO(&set);
	FD_SET(socket_fd, &set);
	FD_SET(signal_handler_pipe[0], &set);
	FD_SET(fd_pipe[0], &set);
	connected_clients_counter = 0; 
	soft_termination_flag = 0;
	hard_termination_flag = 0;
	
	while( hard_termination_flag == 0 && ( soft_termination_flag == 0 ||  connected_clients_counter > 0 ) ){
		rdset = set; // preparo la maschera per select
		if( select(fd_num+1, &rdset, NULL, NULL, NULL) == -1 && errno != EINTR){
			perror("Errore nella select");
			exit(-1);
		}
		if( FD_ISSET(signal_handler_pipe[0], &rdset) ){ // segnale arrivato 
			int signum;
			if( read(signal_handler_pipe[0], &signum, sizeof(int)) == -1 ) signum = SIGINT;
			switch( signum ){
				case SIGINT:
				case SIGQUIT:
					ce_less1(lock(&logfile_lock), "Errore lock logfile");
					fprintf(logfile, "Terminazione brutale\n");
					ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
					hard_termination_flag = 1;
					break;
				case SIGHUP:
					ce_less1(lock(&logfile_lock), "Errore lock logfile");
					fprintf(logfile, "Terminazione soft\n");
					ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
					soft_termination_flag = 1;
					break;
			}
			// posso smettere di ascoltare il signal handler
			FD_CLR(signal_handler_pipe[0], &set);
			if( signal_handler_pipe[0] == fd_num ){
				fd_num -= 1;
			}
			ce_less1(close(signal_handler_pipe[0]), "Errore close pipe signal handler");
			// avendo ricevuto un segnale, sicuramente devo
			// smettere di accettare nuove connessioni in arrivo,
			// quindi posso rimuovere il fd della socket dal
			// set e chiudere il fd che accetta nuove connessioni
			FD_CLR(socket_fd, &set);
			if( socket_fd == fd_num ){
				fd_num -= 1;
			}
			ce_less1(close(socket_fd), "Errore close socket fd");
		}
		// controllo i vari fd
		if( hard_termination_flag != 1 ){
			for(int fd = 0; fd <= fd_num; fd++){
				if( FD_ISSET(fd, &rdset ) && (fd != fd_pipe[0]) && ( fd != signal_handler_pipe[0] ) ){
					if( fd == socket_fd && FD_ISSET(socket_fd,&set) ){ // sock connect pronto
						int fd_connect;

						ce_less1(fd_connect = accept(socket_fd, NULL, 0), "Errore nella accept");
						
						ce_less1(lock(&logfile_lock), "Errore lock logfile");
						fprintf(logfile, "Connessione accettata\n");
						ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
						
						// incremento il contatore dei clients connessi
						connected_clients_counter += 1;
						// aggiorno il set dei fd
						FD_SET(fd_connect, &set);
						if( fd_connect > fd_num ){
							fd_num = fd_connect;
						}
							
					}
					else{ // nuova richiesta di un client connesso
							int *value;
							ce_null(value = malloc(sizeof(int)), "Errore malloc fd");
							*value = fd;
							
							ce_less1(lock(&logfile_lock), "Errore lock logfile");
							fprintf(logfile, "Nuova richiesta client [%d]\n", fd);
							ce_less1(unlock(&logfile_lock), "Errore unlock logfile");
							
							// inoltro la richiesta effettiva ad un worker
							ce_less1(lock(&fd_queue_lock), "Errore lock coda dispatcher/workers");
							ce_null(queueInsert(fd_queue, (void*)value), "Errore inserimento fd coda dispatcher/workers");
							// elimino temporanemaente il fd
							FD_CLR(fd, &set);
							if( fd == fd_num ){
								fd_num -= 1;
							}
							// notifico i workers
							ce_less1(cond_signal(&fd_queue_cond), "Errore cond_signal coda dispatcher/workers");
							ce_less1(unlock(&fd_queue_lock), "Errore unlock coda dispatcher/workers");
					}
				}
			}
		}
		// reinserisco gli eventuali fd serviti dai workers
		if( FD_ISSET(fd_pipe[0], &rdset) ){
			int new_fd;
			if( (readn(fd_pipe[0], (void*)&new_fd, sizeof(int))) == -1 ){
				perror("Errore nella read della pipe di fd");
				hard_termination_flag = 1;
			}
			else{
				if( new_fd == -1 ){
					// decremento il contatore dei clients connessi
					connected_clients_counter -= 1;
				}
				else{ // reinserisco il fd tra quelli da ascoltare
					FD_SET(new_fd, &set);
					if( new_fd > fd_num ){
						fd_num = new_fd;
					}
				}
			}
		}
	}
	// terminazione workers
	terminateWorkers(fd_queue, workers, server_config.thread_workers);
	// stampa stastiche 
	printStats(logfile, server_stats, storage);
	// chiusura dei fd e deallocazione delle varie strutture
	close(socket_fd);
	unlink(server_config.socket_name);
	queueDestroy(fd_queue, free);
	storageDestroy(storage, NULL, fileFree);
	free(server_stats);
	close(fd_pipe[0]);
	close(fd_pipe[1]);
	close(signal_handler_pipe[0]);
	fclose(logfile);
	return 0;
}
