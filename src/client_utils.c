#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "utils.h"
#include "client_utils.h"

void printParams(client_params_t params){
	printf("p_flag: %d\n", params.p_flag);
	printf("delay_milliseconds: %ld\n", params.delay_milliseconds);
	printf("socket_filaname: %s\n", params.socket_filename);
	printQueue(params.requests_queue);
}

int parseOptions(int argc, char *argv[], char optstring[], client_params_t *params){
	int opt;
	char *options = "-f filename -w dirname[,n=0] \
-W file1[,file2] -D dirname -r file1[,file2] \
-R [n=0] -d dirname -t time -l file1[,file2] \
-u file1[,file2] -c file1[,file2] -p";
	option_ptr temp = NULL;
	while( (opt = getopt(argc, argv, optstring)) != -1 ){
		switch(opt){
			case 'h':
				fprintf(stderr, "%s %s\n", argv[0], options);
				return -1;
			case 'f':
				if( params->socket_filename == NULL )
					params->socket_filename = strndup(optarg, MAX_SOCKET_NAME);
				else{
					fprintf(stderr, "Un nome per il socket è già stato definito\n");
					return -1;
				}
				break;
			case 'w':
			case 'D':
			case 'r':
			case 'R':
			case 'd':
			case 't':
			case 'l':
			case 'u':
			case 'c':
				temp = generateNode(opt, strndup(optarg, MAX_ARG_LEN));
				ce_null(temp, "Errore malloc");
				insertNode(&(params->requests_queue), temp);
				break;
			case 'p':
				if( params->p_flag != 0 ){
					fprintf(stderr, "L'opzione -p compare più volte\n");
					return -1;
				}
				else{
					params->p_flag = 1;
				}
				break;
			case '?':
				fprintf(stderr, "Opzione sconosciuta %c\n", optopt);
				return -1;
			case ':':
				if( optopt == 'R' ){
					temp = generateNode(optopt, NULL);
					ce_null(temp, "Errore malloc");
					insertNode(&(params->requests_queue), temp);
				}
				else{
					fprintf(stderr, "L'opzione %c manca di parametri\n", optopt);
				}
				break;
		}
	}
	return 0;
}

/*
	Crea dinamicamente una struct con i parametri passati
	e la restituisce.
	Se è stato impossibile allocare nuova memoria, viene 
	restituito NULL
*/
option_ptr generateNode(char request_opt, char *request_content){
	option_ptr ptr = malloc(sizeof(option));
	if( ptr == NULL ){
		return NULL;
	}
	ptr->request_opt = request_opt;
	ptr->request_content = request_content;
	ptr->next_option_ptr = NULL;
	return ptr;
}

// inserisce il nuovo nodo in coda alla lista
void insertNode(option_ptr *ptr, option_ptr newNode){
	if( *ptr == NULL ){
		*ptr = newNode;
	}
	else{
		option_ptr temp_ptr = *ptr;
		while( temp_ptr->next_option_ptr != NULL){
			temp_ptr = temp_ptr->next_option_ptr;
		}
		temp_ptr->next_option_ptr = newNode;
	}
}

void printQueue(option_ptr ptr){
	if( ptr != NULL){
		printf("Node option %c\n", ptr->request_opt);
		printf("Node content %s\n", ptr->request_content);
		printQueue(ptr->next_option_ptr);
	}
}

/* 
	Libera completamente le memoria allocata per la coda 
   	(oltre alle strutture, anche il campo del contenuto) 
*/ 
void freeQueue(option_ptr *ptr){
	if( *ptr == NULL ){
		return;
	}
	else{
		option_ptr temp_ptr = *ptr;
		*ptr = (*ptr)->next_option_ptr;
		if( temp_ptr->request_content != NULL )
			free(temp_ptr->request_content);
		free(temp_ptr);
		freeQueue(ptr);
	}
}
