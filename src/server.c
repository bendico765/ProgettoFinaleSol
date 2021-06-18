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
#include "socket_utils.h"
#include "thread_utils.h"
#include "config_parser.h"
#include "signal_handler.h"
#include "message.h"

// Mutex e cond. var. della coda tra dispatcher e workers
pthread_mutex_t fd_queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fd_queue_cond = PTHREAD_COND_INITIALIZER;

// Mutex del contatore di clients connessi al server
pthread_mutex_t clients_counter_lock = PTHREAD_MUTEX_INITIALIZER;

// argomenti da passare ai thread workers
struct thread_arg_t{
	queue_t *fd_queue; // coda dei fd da servire
	int fd_pipe; // pipe per restituire i fd serviti al dispatcher
	int *connected_clients_counter; // contatore di clients connessi
	FILE *logfile; // file di log
} thread_arg_t;

void* workerThread(void* arg){
	queue_t *fd_queue = ((struct thread_arg_t*)arg)->fd_queue;
	int fd_pipe =  ((struct thread_arg_t*)arg)->fd_pipe;
	int *connected_clients_counter = ((struct thread_arg_t*)arg)->connected_clients_counter;
	FILE* logfile = ((struct thread_arg_t*)arg)->logfile;
	
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
				// scrivo il fd nella pipe
				ce_less1( writen(fd_pipe, (void*)&fd, sizeof(int)), "Fallimento scrittura pipe thread worker");
				fprintf(logfile, "THREAD: ho scritto fd [%d] nella pipe\n", fd);
			}
			else{
				if( res == -1 ){
					fprintf(logfile, "THREAD: (%s) durante la lettura dell'header di [%d]\n", strerror(errno), fd);
				}
				// rimozione dei dati del client dalle strutture interne
				fprintf(logfile, "THREAD: chiudo la connessione con [%d]\n", fd);
				// decremento il contatore dei clients connessi al server
				ce_less1( lock(&clients_counter_lock), "Fallimento della lock in un thread worker");
				*connected_clients_counter -= 1;
				close(fd);
				ce_less1( unlock(&clients_counter_lock), "Fallimento della unlock in un thread worker");
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

pthread_t* initializeWorkers(int number_workers, struct thread_arg_t *arg_struct){
	pthread_t *workers = malloc(sizeof(pthread_t) * number_workers);
	if( workers != NULL ){
		for(int i = 0; i < number_workers; i++){
			if( (errno = pthread_create(&(workers[i]), NULL, workerThread, (void*)arg_struct)) != 0 )
				return NULL; 
		}
	}
	return workers;
}

int main(int argc, char *argv[]){
	config_t server_config;
	int signal_handler_pipe[2]; // pipe main/signal handler
	int fd_pipe[2]; // pipe dispatcher/workers
	queue_t *fd_queue = NULL; // coda dispatcher/workers
	pthread_t *workers = NULL; // vettore di thread workers
	int socket_fd; // fd del socket del server
	int connected_clients_counter = 0;
	int fd_num = 0; // max fd attimo
	fd_set set; // insieme fd attivi
	fd_set rdset; // insieme fd attesi in lettura
	FILE *logfile; // file in cui stampare i messaggi di log 

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
	
	// inizializzo il pool di workers
	struct thread_arg_t arg_struct = {fd_queue, fd_pipe[1], &connected_clients_counter, logfile};
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
						int fd_connect = accept(socket_fd, NULL, 0);
						ce_less1(fd_connect, "Errore nella accept");
						fprintf(logfile, "Ho accettato una connessione\n");
						lock(&clients_counter_lock);
						connected_clients_counter += 1;
						unlock(&clients_counter_lock);
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
							lock(&fd_queue_lock);
							ce_null(queueInsert(fd_queue, (void*)value), "Errore inserimento fd coda dispatcher/workers");
							// elimino temporanemaente il fd
							FD_CLR(fd, &set);
							if( fd == fd_num ){
								fd_num -= 1;
							}
							// notifico i workers
							cond_signal(&fd_queue_cond);
							unlock(&fd_queue_lock);
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
	close(socket_fd);
	unlink(server_config.socket_name);
	terminateWorkers(fd_queue, workers, server_config.thread_workers);
	queueDestroy(fd_queue, free);
	close(fd_pipe[0]);
	close(fd_pipe[1]);
	close(signal_handler_pipe[0]);
	//fclose(logfile);
	return 0;
}
