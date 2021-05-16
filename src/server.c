#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

#include "utils.h"
#include "queue.h"
#include "socket_utils.h"
#include "thread_utils.h"
#include "config_parser.h"

#define SOCKNAME "mysock"
#define MAX_REQUESTS 5

pthread_mutex_t fd_queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fd_queue_cond = PTHREAD_COND_INITIALIZER;

struct thread_arg_t{
	node_t **fd_queue;
	int *fd_pipe;
} thread_arg_t;

struct signal_handler_arg_t{
	sigset_t *set;
	int *signal_pipe;
} signal_handler_arg_t;

// THREAD SIGNAL HANDLER PERSONALIZZATO
void* threadSignalHandler(void *arg){
	sigset_t *set = ((struct signal_handler_arg_t*)arg)->set;
	int *signal_handler_pipe = ((struct signal_handler_arg_t*)arg)->signal_pipe;
	int signum;
	// il thread rimane in attesa di segnali
	int termination_flag = 0;
	while( termination_flag == 0 ){
		int ret = sigwait(set, &signum);
		if( ret != 0 ){
			errno = ret;
			perror("Errore sigwait");
			return NULL;
		}
		fprintf(stderr, "THREAD: ho appena ricevuto %d\n", signum);
		// gestisco il segnale ricevuto
		switch( signum ){
			case SIGINT:
			case SIGQUIT:
			case SIGHUP:
				termination_flag = 1;
				close(signal_handler_pipe[1]);
				break;
			default:
				break;
		}
	}
	free(set);
	free(arg);
	return NULL;
}

int initializeSignalHandler(int signal_handler_pipe[]){
	sigset_t *set = malloc(sizeof(sigset_t));
	ce_null(set, "Errore malloc");
	// maschero SIGINT. SIGQUIT e SIGHUP
	ce_val(sigemptyset(set), -1, -1);
	ce_val(sigaddset(set, SIGINT), -1, -1);
	ce_val(sigaddset(set, SIGQUIT), -1, -1);
	ce_val(sigaddset(set, SIGHUP), -1, -1);
	ce_not_val(pthread_sigmask(SIG_BLOCK, set, NULL), 0, -1);
	// lancio il thread per il signal handling
	struct signal_handler_arg_t *args = malloc(sizeof(signal_handler_arg_t));
	args->set = set;
	args->signal_pipe = signal_handler_pipe;
	pthread_t thread_id;
	ce_not_val(pthread_create(&thread_id, NULL, &threadSignalHandler, (void*)args), 0, -1);
	ce_not_val(pthread_detach(thread_id), 0, -1);
	return 0;
}

void terminateWorkers(node_t **fd_queue, pthread_t workers[], int number_workers){
	for(int i = 0; i < number_workers; i++){
		lock(&fd_queue_lock);
		insertNode(fd_queue, generateNode(-1));
		cond_signal(&fd_queue_cond);
		unlock(&fd_queue_lock);
	}
	for(int i = 0; i < number_workers; i++){
		pthread_join(workers[i], NULL);
	}
	free(workers);
}

void* workerThread(void* arg){
	node_t **fd_queue = ((struct thread_arg_t*)arg)->fd_queue;
	int *fd_pipe =  ((struct thread_arg_t*)arg)->fd_pipe;
	close(fd_pipe[0]);
		
	int termination_flag = 0;
	while( termination_flag == 0){
		int fd;
		// Sezione critica: prelevo il fd dalla coda
		lock(&fd_queue_lock);
		while( (*fd_queue) == NULL ){ // coda vuota: aspetto
			cond_wait(&fd_queue_cond, &fd_queue_lock);
		}
		node_t *tmp_node = removeNode(fd_queue);
		unlock(&fd_queue_lock);
		fd = tmp_node->value;
		free(tmp_node);
		if( fd == -1 ){
			termination_flag = 1;
		}
		else{
			// servo il file descriptor
			// scrivo il fd nella pipe
			//writen(fd_pipe[1], (void*)&fd, sizeof(int));
		}
	}
	return 0;
}

pthread_t* initializeWorkers(int number_workers, struct thread_arg_t *arg_struct){
	pthread_t *workers = malloc(sizeof(pthread_t) * number_workers);
	if( workers != NULL ){
		for(int i = 0; i < number_workers; i++){
			if( pthread_create(&(workers[i]), NULL, workerThread, (void*)arg_struct) != 0 )
				return NULL; 
		}
	}
	return workers;
}

int main(int argc, char *argv[]){
	config_t server_config;
	int signal_handler_pipe[2]; // pipe main/signal handler
	int fd_pipe[2]; // pipe dispatcher/workers
	node_t *fd_queue = NULL; // coda dispatcher/workers
	pthread_t *workers = NULL; // vettore di thread workers

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

	// inizializzo le pipes
	ce_less1(pipe(signal_handler_pipe), "Errore signal handler pipe");
	ce_less1(pipe(fd_pipe), "Errore threads pipe");
	
	// inizializzo il signal handler
	ce_less1(initializeSignalHandler(signal_handler_pipe), "Errore inizializzazione signal handler");
	
	// inizializzo il pool di workers
	struct thread_arg_t arg_struct = {&fd_queue, fd_pipe};
	ce_less1(pipe(fd_pipe), "Errore pipe dispatcher/threads");
	ce_null(workers = initializeWorkers(server_config.thread_workers, &arg_struct), "Errore nell'inizializzazione dei thread");


	char prova;
	readn(signal_handler_pipe[0], (void*)&prova, sizeof(char));
	terminateWorkers(&fd_queue, workers, server_config.thread_workers);
	close(fd_pipe[0]);
	close(fd_pipe[1]);
	close(signal_handler_pipe[0]);
	return 0;
}
