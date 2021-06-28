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
	new_node->prec_node = NULL;
	
	// inserisce il nuovo nodo in testa alla coda
	if( queue->head_node == NULL ){
		queue->head_node = new_node;
		queue->tail_node = new_node;
	}
	else{
		new_node->next_node = queue->head_node;
		queue->head_node->prec_node = new_node;
		queue->head_node = new_node;
	}
	
	return new_node;
}

/*
	Dealloca tutte le strutture della coda, ed usa la funzione
	free_value per deallocare il contenuto dei nodi.
	Restituisce 0 in caso di successo, -1 altrimenti.
*/
int queueDestroy(queue_t *queue, void (*free_value)(void*)){
	node_t *tmp;
	
	if( queue == NULL ){
		return -1;
	}
	
	while( (tmp = queue->head_node) != NULL ){
		queue->head_node = queue->head_node->next_node;
		if( *free_value ){ 
			if( tmp->value != NULL ) (*free_value)(tmp->value);
		}
		free(tmp);
	}
	free(queue);
	return 0;
}

/*
	Rimuove il nodo in coda e restituisce l'elemento.
	Restituisce NULL nel caso in cui la coda sia vuota
*/
void* queueRemove(queue_t *queue){
	void *value;
	node_t *removed_node;
	
	if( ( removed_node = queue->tail_node) != NULL ){
		if( queue->tail_node == queue->head_node ){
			queue->head_node = NULL;
			queue->tail_node = NULL;
		}
		else{
			queue->tail_node = removed_node->prec_node;
			queue->tail_node->next_node = NULL;
		}
		value = removed_node->value;
		free(removed_node);
		return value;
	}
	return NULL;
}

/*
	Restituisce 0 se la coda è vuota, -1 altrimenti
*/
int queueIsEmpty(queue_t *queue){
	if( queue->head_node == NULL ){
		return 0;
	}
	else{
		return -1;
	}
}

/*
	Rimuove dalla coda la prima occorrenza del valore value,
	partendo dalla fine della coda ed usando la funzione 
	value_compare per confrontare i valori.
	Restituisce l'elemento rimosso, NULL se non
	è stato trovato all'interno della coda
*/
void* queueRemoveFirstOccurrance(queue_t *queue, void* value, int(*value_compare)(void*, void*)){
	node_t *tmp;
	
	if( *value_compare == NULL ) return NULL;
	
	tmp = queue->tail_node;
	while( tmp != NULL ){
		if( value_compare(value, tmp->value) == 0 ){ // elemento trovato
			void *removed_value = tmp->value;
			node_t *right_node = tmp->next_node;
			node_t *left_node = tmp->prec_node;
			// sistemo i puntatori dei nodi a destra ed a sinistra 
			// del nodo con l'elemento trovato
			if( right_node != NULL ){
				right_node->prec_node = tmp->prec_node;
			}
			if( left_node != NULL ){
				left_node->next_node = tmp->next_node;
			}
			// sistemo i puntatori alla testa ed alla coda 
			// della lista
			if( tmp == queue->head_node ){
				queue->head_node = right_node;
			}
			if( tmp == queue->tail_node ){
				queue->tail_node = left_node;
			}
			free(tmp);
			return removed_value;
		}
		else{
			tmp = tmp->prec_node;
		}
	}
	return NULL;
}

/*
	Restituisce il numero di elementi nella coda
*/
int queueLen(queue_t *queue){
	int counter = 0;
	node_t *tmp;
	
	if( queue == NULL ){
		return 0;
	}
	
	tmp = queue->head_node;
	while( tmp != NULL ){
		counter += 1;
		tmp = tmp->next_node;
	}
	return counter;
}

/*
	Restituisce una coda contenente N elementi appartenenti
	al parametro queue, o NULL in caso di errore.
	
	Gli elementi nella coda restituita non sono copie, ma 
	sono gli elementi originali della coda specificata
	come parametro.
*/
queue_t* queueGetNElems(queue_t *queue, int N){
	queue_t *shallow_queue;
	node_t *tmp;
	int counter;
	
	if( queue == NULL || (shallow_queue = queueCreate()) == NULL ) return NULL;
	
	tmp = queue->head_node;
	counter = 0;
	while( tmp != NULL && counter < N){
		if( queueInsert(shallow_queue, tmp->value) == NULL ) return NULL;
		counter += 1;
		tmp = tmp->next_node;
	}
	
	return shallow_queue;
}

