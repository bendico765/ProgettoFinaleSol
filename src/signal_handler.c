#include "signal_handler.h"
#include <utils.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

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
		// gestisco il segnale ricevuto
		switch( signum ){
			case SIGINT:
			case SIGQUIT:
			case SIGHUP:
				termination_flag = 1;
				writen(signal_handler_pipe[1], &signum, sizeof(int));
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
