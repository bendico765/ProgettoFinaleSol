#include "queue.h"
#include <stdlib.h>

/*
	Crea ed inizializza una coda vuota, restituendo
	NULL in caso di errore
*/
queue_t* queueCreate(){
	queue_t *new_queue = malloc(sizeof(queue_t));
	
	if( new_queue == NULL ) return NULL;
	new_queue->head_node = NULL;
	new_queue->tail_node = NULL;
	
	return new_queue;
}

/*
	Inserisce l'elemento value in testa alla coda.
	Restituisce NULL in caso di errore.
*/
node_t* queueInsert(queue_t *queue, void *value){
	if( queue == NULL || value == NULL ) return NULL;
	
	node_t *new_node = malloc(sizeof(node_t));
	if( new_node == NULL ) return NULL;
	new_node->value = value;
	new_node->next_node = NULL;
	
	// inserisce il nuovo nodo in coda alla lista
	if( queue->head_node == NULL ){
		queue->head_node = new_node;
		queue->tail_node = new_node;
	}
	else{
		queue->tail_node->next_node = new_node;
		queue->tail_node = new_node;
	}
	
	return new_node;
}

/*
	Dealloca tutte le strutture della coda, ed usa la funzione
	free_value per deallocare il contenuto dei nodi
*/
void queueDestroy(queue_t *queue, void (*free_value)(void*)){
	node_t *tmp;
	
	while( (tmp = queue->head_node) != NULL ){
		queue->head_node = queue->head_node->next_node;
		(*free_value)(tmp->value);
		free(tmp);
	}
	
	free(queue);
}

/*
	Rimuove dalla coda e restituisce l'elemento in testa.
	Restituisce NULL nel caso in cui la lista sia vuota.
*/
void* queueRemove(queue_t *queue){
	void *value;
	node_t *removed_node;
	
	if( ( removed_node = queue->head_node) != NULL ){
		queue->head_node = removed_node->next_node;
		value = removed_node->value;
		free(removed_node);
		return value;
	}
	return NULL;
}

