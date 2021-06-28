#include "file.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

int fileEdit(file_t *file, void *new_content, size_t new_size){
	if( file == NULL || new_size < 0) return -1;
	
	// dealloco le informazioni precedenti
	if( file->content != NULL ) free(file->content);
	
	// alloco spazio per il nuovo contenuto
	file->content = malloc(new_size);
	if( file->content == NULL ) return -1;
	memcpy(file->content, new_content, new_size);
	file->size = new_size;
	
	return 0;
}

void filePrintInfo(void *file, FILE *stream){
	file_t *tmp = (file_t*) file;
	fprintf(stream, "->%s (%ld bytes)\n", tmp->pathname, tmp->size);
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
