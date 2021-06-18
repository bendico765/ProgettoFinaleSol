#include "file.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

file_t* generateFile(char pathname[], char *content, size_t size){
	file_t *new_file = malloc(sizeof(file_t));
	if( new_file == NULL ){
		return NULL;
	}
	strncpy(new_file->pathname, pathname, PATH_LEN_MAX);
	new_file->content = content;
	new_file->size = size;
	new_file->blank_file_flag = 0;
	return new_file;
}

/*
	Libera la memoria allogata per il file arg (che deve 
	essere passato come puntatore *void). Vengono deallocate
	le code di supporto al file, il contenuto e la struttura
	del file.
*/
void freeFile(void *arg){
	file_t *file = (file_t*)arg;
	if( file != NULL ){
		if( file->content != NULL ) free(file->content);
		free(file);
	}
}
