#include "signal_handler.h"
#include <utils.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

/*
	Thread signal handler personalizzato
*/
void* threadSignalHandler(void *arg){
	sigset_t *set = ((struct signal_handler_arg_t*)arg)->set;
	int *signal_handler_pipe = ((struct signal_handler_arg_t*)arg)->signal_pipe;
	int signum;
	int termination_flag;
	
	// il thread rimane in attesa di segnali
	termination_flag = 0;
	while( termination_flag == 0 ){
		int ret = sigwait(set, &signum);
		if( ret != 0 ){
			errno = ret;
			perror("Errore sigwait");
			signum = SIGINT;
		}
		// gestisco il segnale ricevuto
		switch( signum ){
			case SIGINT:
			case SIGQUIT:
			case SIGHUP:
				termination_flag = 1;
				if( writen(signal_handler_pipe[1], &signum, sizeof(int)) == -1 ) exit(-1);
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

/*
	Funzione per inizializzare e lanciare un thread signal 
	handler.
	
	Il thread è blocca i segnali SIGINT, SIGQUIT e SIGHUP
	ed ignora il segnale SIGPIPE.
	
	Il parametro signal_handler_pipe è una pipe dove
	il signal handler scrive l'intero identificativo
	del segnale ricevuto.
	
	Restituisce 0 nel caso in cui il thread sia lanciato
	con successo, -1 in caso di errore.
*/
int initializeSignalHandler(int signal_handler_pipe[]){
	struct sigaction s;
	struct signal_handler_arg_t *args;
	sigset_t *set;
	
	set = malloc(sizeof(sigset_t));
	if( set == NULL ) return -1;
	
	// maschero SIGINT. SIGQUIT e SIGHUP
	if( sigemptyset(set) == -1 ) return -1;
	if( sigaddset(set, SIGINT) == -1 ) return -1;
	if( sigaddset(set, SIGQUIT) == -1 ) return -1;
	if( sigaddset(set, SIGHUP) == -1 ) return -1;
	if( pthread_sigmask(SIG_BLOCK, set, NULL) != 0 ) return -1;
	
	// ignoro SIGPIPE
	memset(&s, 0, sizeof(s));
	s.sa_handler= SIG_IGN;
	if( sigaction(SIGPIPE,&s,NULL) == -1 ) return -1;
	
	// lancio il thread per il signal handling
	args = malloc(sizeof(signal_handler_arg_t));
	args->set = set;
	args->signal_pipe = signal_handler_pipe;
	pthread_t thread_id;
	if( pthread_create(&thread_id, NULL, &threadSignalHandler, (void*)args) != 0 ) return -1;
	if( pthread_detach(thread_id) != 0 ) return -1;
	
	return 0;
}
