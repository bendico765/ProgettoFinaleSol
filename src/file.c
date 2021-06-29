#include "file.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
	Restituisce un file contenente i parametri passati, o NULL
	in caso di errore.
*/
file_t* fileGenerate(char pathname[], void *content, size_t size){
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

/*
	Restituisce un intero più piccolo, uguale, o più grande di zero 
	se il file1 ha un pathname rispettivamente più piccolo, uguale 
	o maggiore del pathname di file2 (usando l'ordine  lessicografico).
*/
int fileEqual(void *file1, void *file2){
	file_t *f1 = (file_t*)file1;
	file_t *f2 = (file_t*)file2;
	return strcmp(f1->pathname, f2->pathname);
}

/*
	Aggiorna il file passato con il nuovo contenuto new_content di 
	grandezza new_size e dealloca il precedente contenuto.
	
	Restituisce 0 in caso di successo, -1 altrimenti.
*/
int fileEdit(void *file, void *new_content, size_t new_size){
	file_t *casted_file;
	if( file == NULL || new_size < 0) return -1;
	
	casted_file = (file_t*) file;
	// dealloco le informazioni precedenti
	if( casted_file->content != NULL ) free(casted_file->content);
	
	// alloco spazio per il nuovo contenuto
	casted_file->content = malloc(new_size);
	if( casted_file->content == NULL ) return -1;
	memcpy(casted_file->content, new_content, new_size);
	casted_file->size = new_size;
	
	return 0;
}

/*
	La funzione stampa le informazioni sul file passato come 
	parametro sullo stream desiderato.
*/
void filePrintInfo(void *file, FILE *stream){
	file_t *tmp = (file_t*) file;
	fprintf(stream, "->%s (%ld bytes)\n", tmp->pathname, tmp->size);
}


size_t fileGetSize(void *file){
	if( file != NULL ){
		return ((file_t*)file)->size;
	}
	else{
		return 0;
	}
}

/*
	Restituisce 0 se i pathname dei due files passati come parametri
	sono diversi, -1 altrimenti
*/
int fileAreDifferent(void *file1, void *file2){
	if( file1 == NULL || file2 == NULL ) return -1;
	if( strcmp(((file_t*)file1)->pathname, ((file_t*)file2)->pathname) != 0 ) return 0;
	return -1;
}

void* fileGetPathname(void *file){
	return ((file_t*)file)->pathname;
}

/*
	Libera completamente la memoria allogata per il file arg (che deve 
	essere passato come puntatore *void).
*/
void fileFree(void *arg){
	file_t *file = (file_t*)arg;
	if( file != NULL ){
		if( file->content != NULL ) free(file->content);
		free(file);
	}
}
