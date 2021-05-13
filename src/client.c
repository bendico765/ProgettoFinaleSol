#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "client_utils.h"
#include "opt_keys.h"

client_params_t params = {-1,0,0,NULL,NULL}; 

void exitFunction(){
	if( params.socket_filename != NULL ){ free(params.socket_filename); }
	freeQueue(&(params.requests_queue));
}

int main(int argc, char *argv[]){
	ce_less1(atexit(exitFunction), "Errore atexit");
	char optstring[] = ":hf:w:W:D:r:R:d:t:l:u:c:p";
	if( parseOptions(argc, argv, optstring, &params) != 0 ){
		fprintf(stderr, "Chiusura del client\n");
		exit(-1);
	}
	//for(  )
	return 0;
}

/*option_ptr wArgumentParse(char *optarg){
	option_ptr new_request = malloc(sizeof(option));
	ce_null(new_request, "Malloc wArgumentParse");
	char *temp = strtok(optarg, ","); // dirname
	new_request->strndup(new_request->dirname, temp, MAX_DIRNAME_LEN);
	// se è presente un elemento dopo la virgola, è il
	// delimitatore del parametro n
	temp = strtok(NULL, ",");
	if( temp == NULL )
		new_request->max_files_to_send = 0;
	else{
		long number;
		if( isNumber(temp, &number) || number < 0 ){
			fprintf(stderr, "Errore con l'opzione -w\n");
			free(new_request);
			exit(-1);
		}
		new_request->max_files_to_send = number;
	}
	return new_request;
}*/
