#include "queue.h"
#include <stdio.h>
#include <stdlib.h>

/*
	Crea dinamicamente una struct con i parametri passati
	e la restituisce.
	Se è stato impossibile allocare nuova memoria, viene 
	restituito NULL
*/
node_t* generateNode(int value){
	node_t *ptr = malloc(sizeof(node_t));
	if( ptr == NULL ){
		return NULL;
	}
	ptr->value = value;
	ptr->next_node_ptr = NULL;
	return ptr;
}

// inserisce il nuovo nodo in coda alla lista
void insertNode(node_t **ptr, node_t *new_node){
	if( *ptr == NULL ){
		*ptr = new_node;
	}
	else{
		node_t *temp_ptr = *ptr;
		while( temp_ptr->next_node_ptr != NULL){
			temp_ptr = temp_ptr->next_node_ptr;
		}
		temp_ptr->next_node_ptr = new_node;
	}
}

// rimuove il nodo in testa della lista
node_t* removeNode(node_t **ptr){
	if( *ptr == NULL ){
		return NULL;
	}
	else{
		node_t *temp_ptr = *ptr;
		*ptr = (*ptr)->next_node_ptr;
		return temp_ptr;
	}
}

void printQueue(node_t *ptr){
	if( ptr != NULL){
		printf("Value: %d\n", ptr->value);
		printQueue(ptr->next_node_ptr);
	}
}

/* 
	Libera completamente le memoria allocata per la coda  
*/ 
void freeQueue(node_t **ptr){
	if( *ptr == NULL ){
		return;
	}
	else{
		node_t *temp_ptr = *ptr;
		*ptr = (*ptr)->next_node_ptr;
		free(temp_ptr);
		freeQueue(ptr);
	}
}

