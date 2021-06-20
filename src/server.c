#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
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
#include "icl_hash.h"
#include "cache.h"
#include "file.h"

// Mutex e cond. var. della coda tra dispatcher e workers
pthread_mutex_t fd_queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fd_queue_cond = PTHREAD_COND_INITIALIZER;

// Mutex del contatore di clients connessi al server
pthread_mutex_t clients_counter_lock = PTHREAD_MUTEX_INITIALIZER;

// Mutex della struct delle statistiche
pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

// Mutex dello storage
pthread_mutex_t storage_lock = PTHREAD_MUTEX_INITIALIZER;

// Mutex della tabella dei fd
pthread_mutex_t fd_table_lock = PTHREAD_MUTEX_INITIALIZER;

// argomenti da passare ai thread workers
typedef struct{
	icl_hash_t *file_hash_table; // storage dei files
	icl_hash_t *fd_hash_table; // tabella dei fd
	cache_t *cache; // cache fifo per i files
	queue_t *fd_queue; // coda dei fd da servire
	int fd_pipe; // pipe per restituire i fd serviti al dispatcher
	int *connected_clients_counter; // contatore di clients connessi
	FILE *logfile; // file di log
	stats_t *server_stats; // struct delle statistiche
} thread_arg_t;

void freeFdQueue(void *queue){
	queueDestroy((queue_t*)queue, NULL);
}

/*
	Funzione che si occupa di gestire le richieste 
	effettuate tramite openFile()
*/
void openFileHandler(int fd, message_header_t *hdr, FILE *logfile, icl_hash_t *file_hash_table, icl_hash_t *fd_hash_table, cache_t *cache, stats_t *server_stats){
	fprintf(logfile, "THREAD [%ld]: openFile(%s, %d)\n", pthread_self(), hdr->filename, hdr->flags);
	// controllo la validità del flag O_CREATE
	if( hdr->flags != 0 && hdr->flags != 1 ){
		fprintf(logfile, "THREAD [%ld]: Flag non valido\n", pthread_self());
		sendMessageHeader(fd, INVALID_FLAG, NULL, 0);
		return;
	}
	// controllo se O_CREATE = 0 ed il file non esiste
	if( hdr->flags == 0 ){
		file_t *file;
		
		ce_less1( lock(&storage_lock), "Errore lock file_table openFile" );
		file = (file_t*) icl_hash_find(file_hash_table, (void*)hdr->filename);
		ce_less1( unlock(&storage_lock), "Errore unlock file_table openFile" );
		
		if( file == NULL ){
			fprintf(logfile, "THREAD [%ld]: File inesistente\n", pthread_self());
			sendMessageHeader(fd, INEXISTENT_FILE, NULL, 0);
			return;
		}
		else{
			// aggiungo il file alla lista dei files aperti dal client associato al fd
			fprintf(logfile, "THREAD [%ld]: Aggiungo il file alla tabella del fd\n", pthread_self());
			queue_t *files_opened_by_fd;
			ce_less1( lock(&fd_table_lock), "Errore lock fd_table openFile");
			files_opened_by_fd = (queue_t*) icl_hash_find(fd_hash_table, (void*)&fd);
			queueInsert(files_opened_by_fd, (void*)file);
			ce_less1( unlock(&fd_table_lock), "Errore unlock fd_table openFile");
		}
	}
	else{
		// controllo se O_CREATE = 1 ed il file esiste già
		if( icl_hash_find(file_hash_table, (void*)hdr->filename) != NULL ){
			fprintf(logfile, "THREAD [%ld]: File già esistente\n", pthread_self());
			sendMessageHeader(fd, FILE_ALREADY_EXIST, NULL, 0);
			return;
		}
		else{ // creo il file
			/*file_t *new_file;
			queue_t expelled_files;
			queue_t *files_opened_by_fd;
			file_t *expelled_file;
			int err;
			// creo il file vuoto
			ce_null( new_file = generateFile(hdr->filename, NULL, 0), "Errore malloc nuovo file");
	
			// provo ad inserirlo in cache
			ce_less1(lock(&storage_lock), "Errore lock storage");
			expelled_files = insertFileIntoCache(cache, new_file, &err);
			if( err != 0 ){
				perror("Errore nell'inserimento di un nuovo file in cache");
				exit(-1);
			}
			// elimino i files espulsi dallo storage e dalla tabella dei fd
			while( (expelled_file = (file_t*) queueRemove(expelledFiles)) != NULL ){
			
				icl_hash_delete(file_hash_table, (void*)expelled_file->pathname, NULL, freeFile);
			}
			destroyQueue(expelled_files);
			// lo inserisco nel file storage
			ce_null(icl_hash_insert(file_hash_table, (void*)new_file->pathname, (void*)new_file), "Errore inserimento nuovo file hash table");
			// aggiorno la lista dei files aperti dal fd
			fprintf(logfile, "THREAD [%ld]: Aggiungo il file alla tabella del fd\n", pthread_self());
			ce_less1( lock(&fd_table_lock), "Errore lock fd_table openFile");
			files_opened_by_fd = (queue_t*) icl_hash_find(fd_hash_table, (void*)fd);
			queueInsert(files_opened_by_fd, (void*)file);
			// aggiorno le statistiche
			ce_less1(lock(&stats_lock), "Errore lock struttura statistiche");
			server_stats->num_files += 1;
			ce_less1(unlock(&stats_lock), "Errore unlock struttura statistiche");
			ce_less1( unlock(&fd_table_lock), "Errore unlock fd_table openFile");
			ce_less1(unlock(&storage_unlock), "Errore unlock storage");	*/	
		}
	}
	sendMessageHeader(fd, SUCCESS, NULL, 0);
	return;
}

void* workerThread(void* arg){
	icl_hash_t *file_hash_table = ((thread_arg_t*)arg)->file_hash_table;
	icl_hash_t *fd_hash_table = ((thread_arg_t*)arg)->fd_hash_table;
	cache_t *cache = ((thread_arg_t*)arg)->cache;
	queue_t *fd_queue = ((thread_arg_t*)arg)->fd_queue;
	int fd_pipe =  ((thread_arg_t*)arg)->fd_pipe;
	int *connected_clients_counter = ((thread_arg_t*)arg)->connected_clients_counter;
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
			ce_null(hdr, "Fallimento malloc thread worker");
			res = receiveMessageHeader(fd, hdr);
			if( res > 0 ){ // messaggio arrivato correttamente
				// servo il file descriptor
				switch( hdr->option ){
					case OPEN_FILE_OPT:
						openFileHandler(fd, hdr, logfile, file_hash_table, fd_hash_table, cache, server_stats);
						break;
					case CLOSE_FILE_OPT:
						//closeFileHandler
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
			else{
				if( res == -1 ){
					fprintf(logfile, "THREAD [%ld]: (%s) durante la lettura dell'header di [%d]\n", pthread_self(), strerror(errno), fd);
				}
				// rimozione dei dati del client dalle strutture interne
				fprintf(logfile, "THREAD [%ld]: chiudo la connessione con [%d]\n", pthread_self(), fd);
				
				// decremento il contatore dei clients connessi al server
				ce_less1( lock(&clients_counter_lock), "Fallimento della lock in un thread worker");
				*connected_clients_counter -= 1;
				close(fd);
				ce_less1( unlock(&clients_counter_lock), "Fallimento della unlock in un thread worker");
				// rimuovo il fd dalla tabella
				ce_less1(lock(&fd_table_lock), "Errore nella lock della tabella dei fd");
				ce_less1(icl_hash_delete(fd_hash_table, (void*)&fd, free, freeFdQueue), "Errore rimozione fd dalla tabella");
				ce_less1(unlock(&fd_table_lock), "Errore nella unlock della tabella dei fd");
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

void printStats(stats_t *server_stats, icl_hash_t *file_hash_table){
	int k;
	icl_entry_t *entry;
	char *key;
	void *value;
	printf("---------\n");
	printf("NUMERO FILES MEMORIZZATI: %d\n", server_stats->num_files);
	printf("DIMENSIONE FILE STORAGE (MB): %d\n", server_stats->dim_storage);
	printf("MASSIMO NUMERO DI FILES MEMORIZZATI DURANTE L'ESECUZIONE: %d\n", server_stats->max_num_files);
	printf("MASSIMA DIMENSIONE DELLO STORAGE RAGGIUNTA DURANTE L'ESECUZIONE: %d\n", server_stats->max_dim_storage);
	printf("NUMERO DI VOLTE IN CUI L'ALGORITMO DI CACHING È ENTRATO IN FUNZIONE: %d\n", server_stats->cache_substitutions);
	printf("FILES MEMORIZZATI A FINE ESECUZIONE:\n");
	icl_hash_foreach(file_hash_table, k, entry, key, value)
		printf("%s\n", key);
	printf("---------\n");
}

int main(int argc, char *argv[]){
	config_t server_config;
	int signal_handler_pipe[2]; // pipe main/signal handler
	int fd_pipe[2]; // pipe dispatcher/workers
	queue_t *fd_queue; // coda dispatcher/workers
	pthread_t *workers; // vettore di thread workers
	icl_hash_t *file_hash_table; // tabella hash per lo storage
	icl_hash_t *fd_hash_table; // tabella per associare a ciascun fd i files aperti
	FILE *logfile; // file in cui stampare i messaggi di log 
	stats_t *server_stats; // struct contenente le statistiche
	cache_t *cache; // cache FIFO per i file
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
	file_hash_table = icl_hash_create(server_config.num_buckets_file, NULL, NULL);
	ce_null(file_hash_table, "Errore nell'inizializzazione dello storage");
	
	// inizializzo la tabella dei fd
	fd_hash_table = icl_hash_create(server_config.num_buckets_file, NULL, NULL);
	ce_null(fd_hash_table, "Errore nell'inizializzazione della tabella dei fd");
	
	// inizializzo la cache
	cache = createCache(server_config.max_memory, server_config.max_num_files);
	ce_null(cache, "Errore nell'inizializzazione della cache");
	
	// inizializzo la struct delle statistiche
	ce_null( server_stats = malloc(sizeof(stats_t)), "Errore creazione struct stats");
	
	// inizializzo il pool di workers
	thread_arg_t arg_struct = {file_hash_table, fd_hash_table, cache, fd_queue, fd_pipe[1], &connected_clients_counter, logfile, server_stats};
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
						int *fd_connect;
						queue_t *opened_files;
						
						ce_null(fd_connect = malloc(sizeof(int)), "Errore malloc nuovo fd");
						ce_null(opened_files = queueCreate(), "Errore creazione coda nuovo fd");
						ce_less1((*fd_connect) = accept(socket_fd, NULL, 0), "Errore nella accept");
						
						fprintf(logfile, "Ho accettato una connessione\n");
						// aggiungo il fd alla tabella dei fd
						ce_less1(lock(&fd_table_lock), "Errore nella lock della tabella dei fd");
						ce_null(icl_hash_insert(fd_hash_table, (void*)fd_connect, (void*)opened_files), "Errore inserimento fd nella tabella");
						ce_less1(unlock(&fd_table_lock), "Errore nella unlock della tabella dei fd");
						// incremento il contatore dei clients connessi
						ce_less1(lock(&clients_counter_lock), "Errore nella lock del clients counter");
						connected_clients_counter += 1;
						ce_less1(unlock(&clients_counter_lock), "Errore nella unlock del clients counter");
						// aggiorno il set dei fd
						FD_SET(*fd_connect, &set);
						if( *fd_connect > fd_num ){
							fd_num = *fd_connect;
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
				FD_SET(new_fd, &set);
				if( new_fd > fd_num ){
					fd_num = new_fd;
				}
			}
		}
	}
	// stampa stastiche 
	printStats(server_stats, file_hash_table);
	// chiusura dei fd e deallocazione delle varie strutture
	close(socket_fd);
	unlink(server_config.socket_name);
	terminateWorkers(fd_queue, workers, server_config.thread_workers);
	queueDestroy(fd_queue, free);
	destroyCache(cache);
	icl_hash_destroy(file_hash_table, NULL, freeFile);
	icl_hash_destroy(fd_hash_table, free, freeFdQueue);
	free(server_stats);
	close(fd_pipe[0]);
	close(fd_pipe[1]);
	close(signal_handler_pipe[0]);
	//fclose(logfile);
	return 0;
}
