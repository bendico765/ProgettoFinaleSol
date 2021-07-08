#include "utils.h"
#include "config_parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define BUFFERSIZE 150

cache_type_t getCacheTypeCode(char *cache_type){
	if( strcmp("FIFO", cache_type) == 0 ) return FIFO;
	if( strcmp("LRU", cache_type) == 0 ) return LRU;
	return INVALID;
}

/*
	La funzione legge i parametri di configurazione del server
	dal file identificato da pathname e li salva nella struttura
	puntata da server_config.

	Ritorna:
	 0: lettura terminata con successo
	-1: impossibile aprire file
	-2: errore durante il parsing del file
*/
int parseConfigFile(char *pathname, config_t *server_config){
	char buffer[BUFFERSIZE];
	char *saveptr;
	FILE *file;
	
	if( (file = fopen(pathname, "r")) == NULL ) return -1; 
	
	saveptr = NULL;
	while( fgets(buffer, BUFFERSIZE, file) != NULL ){
		char *param = strtok_r(buffer, "=", &saveptr);
		char *value = strtok_r(NULL, "\n", &saveptr);
		if( param == NULL || value == NULL ){
			return -2;
		}
		if( strcmp(param, "THREAD_WORKERS") == 0 ){ 
			if( isIntNumber(value, &(server_config->thread_workers)) != 0 ) return -2;
			continue;
		}
		if( strcmp(param, "MAX_MEMORY") == 0 ){
			if( isIntNumber(value, &(server_config->max_memory)) != 0 ) return -2;
			continue;
		}
		if( strcmp(param, "MAX_NUM_FILES") == 0 ){
			if( isIntNumber(value, &(server_config->max_num_files)) != 0 ) return -2;
			continue;
		}
		if( strcmp(param, "MAX_CONNECTIONS") == 0 ){
			if( isIntNumber(value, &(server_config->max_connections)) != 0 ) return -2;
			continue;
		}
		if( strcmp(param, "SOCKET_NAME") == 0 ){ 
			if( strlen(value)+1 > UNIX_PATH_MAX ) return -2;
			strcpy(server_config->socket_name, value);
			continue;
		}
		if( strcmp(param, "LOG_FILENAME") == 0 ){ 
			if( strlen(value)+1 > PATH_LEN_MAX ) return -2;
			strcpy(server_config->log_filename, value);
			continue;
		}
		if( strcmp(param, "NUM_BUCKETS_FILE") == 0 ){
			if( isIntNumber(value, &(server_config->num_buckets_file)) != 0 ) return -2;
			continue;
		}
		if( strcmp(param, "CACHE_POLICY") == 0 ){
			server_config->cache_type = getCacheTypeCode(value);
			continue;
		}
		memset(buffer, '\0', BUFFERSIZE);
	}
	fclose(file);
	return 0;
}
