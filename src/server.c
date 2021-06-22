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

#include "utils.h"
#include "queue.h"
#include "stats.h"
#include "socket_utils.h"
#include "thread_utils.h"
#include "config_parser.h"
#include "signal_handler.h"
#include "message.h"
#include "storage.h"
#include "file.h"

// Mutex e cond. var. della coda tra dispatcher e workers
pthread_mutex_t fd_queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fd_queue_cond = PTHREAD_COND_INITIALIZER;

// Mutex della struct delle statistiche
pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

// Mutex dello storage
pthread_mutex_t storage_lock = PTHREAD_MUTEX_INITIALIZER;

// argomenti da passare ai thread workers
typedef struct{
	storage_t *storage; // storage dei files
	queue_t *fd_queue; // coda dei fd da servire
	int fd_pipe; // pipe per restituire i fd serviti al dispatcher
	FILE *logfile; // file di log
	stats_t *server_stats; // struct delle statistiche
} thread_arg_t;

void openFileHandler(int fd, message_header_t *hdr, FILE *logfile, storage_t *storage, stats_t *server_stats){
	fprintf(logfile, "THREAD [%ld]: openFile(%s, %d)\n", pthread_self(), hdr->filename, hdr->flags);
	// controllo la validità del flag O_CREATE
	if( hdr->flags != 0 && hdr->flags != 1 ){
		fprintf(logfile, "THREAD [%ld]: Flag non valido\n", pthread_self());
		sendMessageHeader(fd, INVALID_FLAG, NULL, 0);
		return;
	}
	// controllo se O_CREATE = 0 ed il file non esiste
	file_t *file;
	ce_less1( lock(&storage_lock), "Errore lock storage openFile" );
	file = (file_t*) storageFind(storage, hdr->filename);
	if( hdr->flags == 0 ){
		if( file == NULL ){
			fprintf(logfile, "THREAD [%ld]: File inesistente\n", pthread_self());
			sendMessageHeader(fd, INEXISTENT_FILE, NULL, 0);
		}
		else{
			// imposto che il file è stato appena aperto
			file->is_open = 1;
			sendMessageHeader(fd, SUCCESS, NULL, 0);
		}
	}
	else{
		// controllo se O_CREATE = 1 ed il file esiste già
		if( file != NULL ){
			fprintf(logfile, "THREAD [%ld]: File già esistente\n", pthread_self());
			sendMessageHeader(fd, FILE_ALREADY_EXIST, NULL, 0);
		}
		else{ // creo il file e lo apro
			file_t *new_file;
			queue_t *expelled_files;
			int num_expelled_files;
			int err = 0;
			
			new_file = generateFile(hdr->filename, NULL, 0);
			ce_null(new_file, "Errore generazione nuovo file");
			
			// inserisco il file nello storage
			expelled_files = storageInsert(storage, (void*)new_file->pathname, (void*)new_file, &err);
			if( err != 0 ){
				perror("Errore nell'inserimento in cache durante openFile");
				exit(-1);
			}
			num_expelled_files = queueLen(expelled_files);
			if( num_expelled_files > 0 ){
				fprintf(logfile, "THREAD [%ld]: Ho espulso %d files dallo storage\n", pthread_self(), num_expelled_files);
			}
			
			// aggiorno le statistiche
			ce_less1( lock(&stats_lock), "Errore lock statistiche openFile" );
			server_stats->num_files += 1;
			if( server_stats->num_files > server_stats->max_num_files ) server_stats->max_num_files = server_stats->num_files;
			if( num_expelled_files > 0 ) server_stats->cache_substitutions += 1;			
			ce_less1( unlock(&stats_lock), "Errore unlock statistiche openFile" );



			// dealloco gli eventuali files espulsi
			queueDestroy(expelled_files, freeFile);
			sendMessageHeader(fd, SUCCESS, NULL, 0);
		}
	}
	ce_less1( unlock(&storage_lock), "Errore unlock storage openFile" );
	return;
}

void closeFileHandler(int fd, message_header_t *hdr, FILE *logfile, storage_t *storage){
	fprintf(logfile, "THREAD [%ld]: closeFile(%s)\n", pthread_self(), hdr->filename);
	file_t *file;
	
	ce_less1( lock(&storage_lock), "Errore lock storage closeFile" );
	file = (file_t*) storageFind(storage, hdr->filename);
	if( file == NULL ){
		fprintf(logfile, "THREAD [%ld]: File inesistente\n", pthread_self());
		sendMessageHeader(fd, INEXISTENT_FILE, NULL, 0);
	}
	else{
		file->freshly_opened = 0;
		file->is_open = 0;
		sendMessageHeader(fd, SUCCESS, NULL, 0);
	}
	ce_less1( unlock(&storage_lock), "Errore unlock storage closeFile" );
	return;
}

void removeFileHandler(int fd, message_header_t *hdr, FILE *logfile, storage_t *storage, stats_t *server_stats){
	fprintf(logfile, "THREAD [%ld]: removeFile(%s)\n", pthread_self(), hdr->filename);
	file_t *file;
	
	ce_less1( lock(&storage_lock), "Errore lock storage closeFile" );
	file = (file_t*) storageFind(storage, hdr->filename);
	if( file == NULL ){
		fprintf(logfile, "THREAD [%ld]: File inesistente\n", pthread_self());
		sendMessageHeader(fd, INEXISTENT_FILE, NULL, 0);
	}
	else{
		// rimuovo il file dallo storage
		storageRemove(storage, (void*)hdr->filename, NULL, freeFile);
		
		// aggiorno le statistiche
		ce_less1( lock(&stats_lock), "Errore lock statistiche removeFile" );
		server_stats->num_files -= 1;
		server_stats->dim_storage -= file->size;
		ce_less1( unlock(&stats_lock), "Errore unlock statistiche removeFile" );
		
		sendMessageHeader(fd, SUCCESS, NULL, 0);
	}
	ce_less1( unlock(&storage_lock), "Errore unlock storage closeFile" );
	return;
}

void* workerThread(void* arg){
	storage_t *storage = ((thread_arg_t*)arg)->storage;
	queue_t *fd_queue = ((thread_arg_t*)arg)->fd_queue;
	int fd_pipe =  ((thread_arg_t*)arg)->fd_pipe;
	FILE* logfile = ((thread_arg_t*)arg)->logfile;
	stats_t *server_stats = ((thread_arg_t*)arg)->server_stats;
	
	int termination_flag = 0;
	while( termination_flag == 0){
		int fd;
		void *tmp;
		// Sezione critica: prelevo il fd dalla coda
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
			message_header_t *hdr = malloc(sizeof(message_header_t));
			ce_null(hdr, "Fallimento malloc header in thread worker");
			res = receiveMessageHeader(fd, hdr);
			if( res > 0 ){ // messaggio arrivato correttamente
				// servo il file descriptor
				switch( hdr->option ){
					case OPEN_FILE_OPT:
						openFileHandler(fd, hdr, logfile, storage, server_stats);
						break;
					case CLOSE_FILE_OPT:
						closeFileHandler(fd, hdr, logfile, storage);
						break;
					case READ_FILE_OPT:
						break;
					case READ_N_FILE_OPT:
						break;
					case WRITE_FILE_OPT:
						break;
					case APPEND_TO_FILE_OPT:
						break;
					case REMOVE_FILE_OPT:
						removeFileHandler(fd, hdr, logfile, storage, server_stats);
						break;
					default:
						fprintf(logfile, "THREAD [%ld]: opzione invalida\n", pthread_self());
						sendMessageHeader(fd, INVALID_OPTION, NULL, 0);
						break;
				
				}
				// scrivo il fd nella pipe
				ce_less1( writen(fd_pipe, (void*)&fd, sizeof(int)), "Fallimento scrittura pipe thread worker");
				fprintf(logfile, "THREAD [%ld]: ho scritto fd [%d] nella pipe\n", pthread_self(), fd);
			}
			else{ // rimozione dei dati del client dalle strutture interne
				int tmp = -1;
				if( res == -1 ){
					fprintf(logfile, "THREAD [%ld]: (%s) durante la lettura dell'header di [%d]\n", pthread_self(), strerror(errno), fd);
				}
				fprintf(logfile, "THREAD [%ld]: chiudo la connessione con [%d]\n", pthread_self(), fd);
				
				// comunico la disconnessione di un client
				ce_less1( writen(fd_pipe, (void*)&tmp, sizeof(int)), "Fallimento scrittura pipe thread worker");
				// chiudo l'effettivo fd, rendendolo nuovamente disponibile al dispatcher
				close(fd);
			}
			free(hdr);
		}
	}
	return 0;
}

void terminateWorkers(queue_t *fd_queue, pthread_t workers[], int number_workers){
	for(int i = 0; i < number_workers; i++){
		int *value = malloc(sizeof(int));
		ce_null(value, "Errore malloc terminateWorkers");
		*value = -1;
		ce_less1( lock(&fd_queue_lock), "Errore della lock nella terminazione dei workers");
		ce_null(queueInsert(fd_queue, (void*)value), "Errore queueInsert nella terminazione dei workers");
		ce_less1( cond_signal(&fd_queue_cond), "Errore nella signal della terminazione dei workers");
		ce_less1( unlock(&fd_queue_lock), "Errore nella unlock della terminazione dei workers");
	}
	for(int i = 0; i < number_workers; i++){
		errno = pthread_join(workers[i], NULL);
		if( errno != 0 ){
			perror("Errore nella pthread_join dei workers");
			errno = 0;
			return;
		}
	}
	free(workers);
}

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

void printStats(stats_t *server_stats, storage_t *storage){
	printf("---------\n");
	printf("NUMERO FILES MEMORIZZATI: %d\n", server_stats->num_files);
	printf("DIMENSIONE FILE STORAGE (MB): %d\n", server_stats->dim_storage);
	printf("MASSIMO NUMERO DI FILES MEMORIZZATI DURANTE L'ESECUZIONE: %d\n", server_stats->max_num_files);
	printf("MASSIMA DIMENSIONE DELLO STORAGE RAGGIUNTA DURANTE L'ESECUZIONE: %d\n", server_stats->max_dim_storage);
	printf("NUMERO DI VOLTE IN CUI L'ALGORITMO DI CACHING È ENTRATO IN FUNZIONE: %d\n", server_stats->cache_substitutions);
	printf("FILES MEMORIZZATI A FINE ESECUZIONE:\n");
	storagePrint(storage);
	printf("---------\n");
}

int main(int argc, char *argv[]){
	storage_t *storage; // storage dei files
	config_t server_config;
	int signal_handler_pipe[2]; // pipe main/signal handler
	int fd_pipe[2]; // pipe dispatcher/workers
	queue_t *fd_queue; // coda dispatcher/workers
	pthread_t *workers; // vettore di thread workers
	FILE *logfile; // file in cui stampare i messaggi di log 
	stats_t *server_stats; // struct contenente le statistiche
	int socket_fd; // fd del socket del server
	int connected_clients_counter = 0;
	int fd_num = 0; // max fd attimo
	fd_set set; // insieme fd attivi
	fd_set rdset; // insieme fd attesi in lettura

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
	//ce_null(logfile = fopen(server_config.log_filename, "a"), "Errore nell'apertura del file di log");
	logfile = stderr;
	
	// inizializzo la coda dispatcher/workers
	ce_null(fd_queue = queueCreate(), "Errore creazione coda dispatcher/workers");
	
	// inizializzo le pipes
	ce_less1(pipe(signal_handler_pipe), "Errore signal handler pipe");
	ce_less1(pipe(fd_pipe), "Errore pipe dispatcher/threads");
	
	// inizializzo il signal handler
	ce_less1(initializeSignalHandler(signal_handler_pipe), "Errore inizializzazione signal handler");
	
	// inizializzo lo storage
	storage = storageCreate(server_config.num_buckets_file, NULL, NULL, server_config.max_memory, server_config.max_num_files);
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
	
	fprintf(logfile, "Server avviato\n");
	fprintf(logfile, "In attesa di connessioni...\n");
	// metto il massimo indice di descrittore 
	// attivo in fd_num
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
	int soft_termination_flag = 0;
	int hard_termination_flag = 0;
	while( hard_termination_flag == 0 && ( soft_termination_flag == 0 ||  connected_clients_counter > 0 ) ){
		rdset = set; // preparo la maschera per select
		if( select(fd_num+1, &rdset, NULL, NULL, NULL) == -1 && errno != EINTR){
			perror("Errore nella select");
			exit(-1);
		}
		if( FD_ISSET(signal_handler_pipe[0], &rdset) ){ // segnale arrivato 
			int signum;
			ce_val_do(read(signal_handler_pipe[0], &signum, sizeof(int)), -1, signum = SIGINT);
			switch( signum ){
				case SIGINT:
				case SIGQUIT:
					fprintf(logfile, "Terminazione brutale\n");
					hard_termination_flag = 1;
					break;
				case SIGHUP:
					fprintf(logfile, "Terminazione soft\n");
					soft_termination_flag = 1;
					break;
			}
			// posso smettere di ascoltare il signal handler
			FD_CLR(signal_handler_pipe[0], &set);
			if( signal_handler_pipe[0] == fd_num ){
				fd_num -= 1;
			}
			close(signal_handler_pipe[0]);
			// avendo ricevuto un segnale, sicuramente devo
			// smettere di accettare nuove connessioni in arrivo,
			// quindi posso rimuovere il fd della socket dal
			// set
			FD_CLR(socket_fd, &set);
			if( socket_fd == fd_num ){
				fd_num -= 1;
			}
		}
		// controllo i vari fd
		if( hard_termination_flag != 1 )
			for(int fd = 0; fd <= fd_num; fd++){
				if( FD_ISSET(fd, &rdset ) && (fd != fd_pipe[0]) && ( fd != signal_handler_pipe[0] ) ){
					if( fd == socket_fd ){ // sock connect pronto
						int fd_connect;

						ce_less1(fd_connect = accept(socket_fd, NULL, 0), "Errore nella accept");
						
						fprintf(logfile, "Ho accettato una connessione\n");
						// aggiungo il fd alla tabella dei fd
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
							fprintf(logfile, "Nuova richiesta client [%d]\n", fd);
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
		// reinserisco gli eventuali fd serviti dai workers
		if( FD_ISSET(fd_pipe[0], &rdset ) ){
			int new_fd;
			int read_chars;
			read_chars = readn(fd_pipe[0], (void*)&new_fd, sizeof(int));
			if( read_chars == -1 ){
				perror("Errore nella read della pipe di fd");
				hard_termination_flag = 1;
			}
			else{
				if( new_fd == -1 ){
					// decremento il contatore dei clients connessi
					connected_clients_counter -= 1;
				}
				else{
					FD_SET(new_fd, &set);
					if( new_fd > fd_num ){
						fd_num = new_fd;
					}
				}
			}
		}
	}
	// stampa stastiche 
	printStats(server_stats, storage);
	// chiusura dei fd e deallocazione delle varie strutture
	close(socket_fd);
	unlink(server_config.socket_name);
	terminateWorkers(fd_queue, workers, server_config.thread_workers);
	queueDestroy(fd_queue, free);
	storageDestroy(storage, NULL, freeFile);
	free(server_stats);
	close(fd_pipe[0]);
	close(fd_pipe[1]);
	close(signal_handler_pipe[0]);
	//fclose(logfile);
	return 0;
}
