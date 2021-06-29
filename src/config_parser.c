#include "utils.h"
#include "config_parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define BUFFERSIZE 150

/*
	Ritorna:
	-1: impossibile aprire file
	-2: errore durante il parsing del file
*/
int parseConfigFile(char *filename, config_t *server_config){
	char buffer[BUFFERSIZE];
	FILE *file;
	
	if( (file = fopen(filename, "r")) == NULL ){ return -1; }
	char *saveptr = NULL;
	while( fgets(buffer, BUFFERSIZE, file) != NULL ){
		char *param = strtok_r(buffer, "=", &saveptr);
		char *value = strtok_r(NULL, "\n", &saveptr);
		if( param == NULL || value == NULL ){
			return -2;
		}
		if( strcmp(param, "THREAD_WORKERS") == 0 ){ 
			if( isIntNumber(value, &(server_config->thread_workers)) != 0 ){
				return -2;
			}
			continue;
		}
		if( strcmp(param, "MAX_MEMORY") == 0 ){
			if( isIntNumber(value, &(server_config->max_memory)) != 0 ){
				return -2;
			}
				continue;
			}
		if( strcmp(param, "MAX_NUM_FILES") == 0 ){
			if( isIntNumber(value, &(server_config->max_num_files)) != 0 ){
				return -2;
			}
			continue;
		}
		if( strcmp(param, "MAX_CONNECTIONS") == 0 ){
			if( isIntNumber(value, &(server_config->max_connections)) != 0 ){
				return -2;
			}
			continue;
		}
		if( strcmp(param, "SOCKET_NAME") == 0 ){ 
			if( strlen(value)+1 > UNIX_PATH_MAX ){
				return -2;
			}
			strcpy(server_config->socket_name, value);
			continue;
		}
		if( strcmp(param, "LOG_FILENAME") == 0 ){ 
			if( strlen(value)+1 > PATH_LEN_MAX ){
				return -2;
			}
			strcpy(server_config->log_filename, value);
			continue;
		}
		if( strcmp(param, "NUM_BUCKETS_FILE") == 0 ){
			if( isIntNumber(value, &(server_config->num_buckets_file)) != 0 ){
				return -2;
			}
			continue;
		}
		memset(buffer, '\0', BUFFERSIZE);
	}
	fclose(file);
	return 0;
}
