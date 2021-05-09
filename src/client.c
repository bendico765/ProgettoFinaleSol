#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "opt_keys.h"

#define MAX_DIRNAME_LEN 128
#define MAX_FILENAME_REQUEST 256
#define MAX_SOCKET_NAME 128

// struct per gestire le richieste dell'opzione -w
typedef struct{
	char dirname[MAX_DIRNAME_LEN];
	// massimo numero di files della cartella 
	// dirname da mandare
	long max_files_to_send;
}write_dir_request;

int p_flag;
long delay_milliseconds = 0; // ritardo tra le richieste
// nome del socket AF_UNIX
char *socket_filename = NULL;
// array contenente le informazioni sulle richieste di 
// lettura fatte col parametro -w 
write_dir_request *requests_array = NULL;
// massimo numero di files da leggere dal server
long max_files_to_read = 0;
// nome della cartella in cui salvare eventuali
// files buttati dal server (opzione -D)
char capacity_miss_dirname[MAX_DIRNAME_LEN];
// cartella in cui scrivere i files letti da server
// (opzione -d)
char to_receive_dirname[MAX_DIRNAME_LEN];
// lista di nomi di file da scrivere nel server; 
// la lista è terminata da NULL
char **to_send_list = NULL;
// lista di nomi di file su cui acquisire la mutua esclusione
// la lista è terminata da NULL
char **to_lock_list = NULL;
// lista di nomi di file su cui rilasciare la mutua 
// esclusione; la lista è terminata da NULL
char **to_unlock_list = NULL;
// lista di file da rimuovere dal server se presenti
// la lista è terminata da NULL
char **to_remove_list = NULL;

void exitFunction(){
	if( socket_filename != NULL ){ free(socket_filename); }
}

write_dir_request* wArgumentParse(char *optarg){
	write_dir_request *newRequest = malloc(sizeof(write_dir_request));
	ce_null(newRequest, "Malloc write_dir_request");
	char *temp = strtok(optarg, ",");
	strncpy(newRequest->dirname, temp, MAX_DIRNAME_LEN);
	// se è presente un elemento dopo la virgola, è il
	// delimitatore del parametro n
	temp = strtok(NULL, ",");
	if( temp == NULL )
		newRequest->max_files_to_send = 0;
	else{
		long number;
		if( isNumber(temp, &number) || number < 0 ){
			fprintf(stderr, "Errore con l'opzione -w\n");
			free(newRequest);
			exit(-1);
		}
		newRequest->max_files_to_send = number;
	}
	return newRequest;
}

int main(int argc, char *argv[]){
	ce_less1(atexit(exitFunction), "Errore atexit");
	
	int opt;
	char optstring[] = ":hf:w:";
	char *options = "-f filename -w dirname[,n=0] \
-W file1[,file2] -D dirname -r file1[,file2] \
-R [n=0] -d dirname -t time -l file1[,file2] \
-u file1[,file2] -c file1[,file2] -p";
	write_dir_request *newRequest;
	while( (opt = getopt(argc, argv, optstring)) != -1 ){
		switch(opt){
			case 'h':
				fprintf(stderr, "%s %s\n", argv[0], options);
				return 0;
			case 'f':
				if( socket_filename == NULL )
					socket_filename = strndup(optarg, MAX_SOCKET_NAME);
				else
					fprintf(stderr, "Un nome per il socket è già stato definito\n");
				break;
			case 'w':
				newRequest = wArgumentParse(optarg);
				break;
			case 'D':
				strncpy(capacity_miss_dirname, optarg, MAX_DIRNAME_LEN);
				break;
			case 'R':
				if( isNumber(optarg, &max_files_to_read) != 0 || max_files_to_read < 0){
					fprintf(stderr, "Errore con l'opzione -R\n");
					exit(-1);
				}
				break;
			case 'd':
				strncpy(to_receive_dirname, optarg, MAX_DIRNAME_LEN);
				break;
			case 't':
				if( isNumber(optarg, &delay_milliseconds) != 0 || delay_milliseconds < 0){
					fprintf(stderr, "Errore con l'opzione -t\n");
					exit(-1);
				}
			case '?':
				fprintf(stderr, "Opzione sconosciuta %c\n", optopt);
				return -1;
			case ':':
				fprintf(stderr, "L'opzione %c manca di parametri\n", optopt);
				return -1;
		}
	}
	
	return 0;
}
