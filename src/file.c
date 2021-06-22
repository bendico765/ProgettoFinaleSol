#include "file.h"
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
	new_file->freshly_opened = 1;
	new_file->is_open = 1;
	return new_file;
}

int fileEqual(void *file1, void *file2){
	file_t *f1 = (file_t*)file1;
	file_t *f2 = (file_t*)file2;
	return strcmp(f1->pathname, f2->pathname);
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
