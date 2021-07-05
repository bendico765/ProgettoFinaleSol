#include "queue.h"
#include <stdlib.h>
#include <time.h>

/*
	Crea ed inizializza una coda vuota, restituendo
	NULL in caso di errore
*/
queue_t* queueCreate(){
	queue_t *new_queue = malloc(sizeof(queue_t));
	
	if( new_queue == NULL ) return NULL;
	new_queue->head_node = NULL;
	new_queue->tail_node = NULL;
	new_queue->len = 0;
		
	return new_queue;
}

/*
	Inserisce l'elemento value in testa alla coda.
	Restituisce NULL in caso di errore.
*/
node_t* queueInsert(queue_t *queue, void *value){
	node_t *new_node;
	
	if( queue == NULL ) return NULL;
	if( (new_node = malloc(sizeof(node_t))) == NULL ) return NULL;
	
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
	
	queue->len += 1;
	
	return new_node;
}

/*
	Sposta il nodo passato come parametro in testa alla coda
	( assumento che il nodo faccia parte della coda ).
	Restituisce 0 in caso di successo, -1 altrimenti.
*/
int queueReinsert(queue_t *queue, node_t *node){
	node_t *right_node;
	node_t *left_node;
	
	if( queue == NULL || node == NULL ) return -1;
	
	right_node = node->next_node;
	left_node = node->prec_node;
	// sistemo i puntatori dei nodi a destra ed a sinistra 
	// del nodo con l'elemento trovato
	if( right_node != NULL ){
		right_node->prec_node = node->prec_node;
	}
	if( left_node != NULL ){
		left_node->next_node = node->next_node;
	}
	// sistemo i puntatori alla testa ed alla coda 
	// della lista
	if( node == queue->head_node ){
		queue->head_node = right_node;
	}
	if( node == queue->tail_node ){
		queue->tail_node = left_node;
	}
	// reinserisco il nodo rimosso alla testa della lista
	if( queue->head_node == NULL ){
		queue->head_node = node;
		queue->tail_node = node;
	}
	else{
		node->next_node = queue->head_node;
		queue->head_node->prec_node = node;
		queue->head_node = node;
	}
	
	return 0;
}

/*
	Dealloca tutto il contenuto della coda, ed usa la funzione
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
	
	if( queue == NULL ) return NULL;
	
	if( ( removed_node = queue->tail_node) != NULL ){
		if( queue->tail_node == queue->head_node ){ // vi è un solo nodo nella lista
			queue->head_node = NULL;
			queue->tail_node = NULL;
		}
		else{
			queue->tail_node = removed_node->prec_node;
			queue->tail_node->next_node = NULL;
		}
		queue->len -= 1;
		value = removed_node->value;
		free(removed_node);
		return value;
	}
	return NULL;
}

/*
	Rimuove il nodo passato come parametro dalla coda,
	gestendo la logica dei puntatori dell lista ed i
	contatori interni.
	
	Nota bene: il nodo passato come parametro viene deallocato.
	
	Restituisce il contenuto del nodo rimosso in caso
	di successo, NULL altrimenti.
*/
void* queueRemoveByNode(queue_t *queue, node_t *node){
	void *removed_value;
	node_t *right_node;
	node_t *left_node;
			
	if( queue == NULL || node == NULL ) return NULL;
	
	removed_value = node->value;
	right_node = node->next_node;
	left_node = node->prec_node;
	// sistemo i puntatori dei nodi a destra ed a sinistra 
	// del nodo con l'elemento trovato
	if( right_node != NULL ){
		right_node->prec_node = node->prec_node;
	}
	if( left_node != NULL ){
		left_node->next_node = node->next_node;
	}
	// sistemo i puntatori alla testa ed alla coda 
	// della lista
	if( node == queue->head_node ){
		queue->head_node = right_node;
	}
	if( node == queue->tail_node ){
		queue->tail_node = left_node;
	}
	queue->len -= 1;
	free(node);
	return removed_value;
}

/*
	Rimuove dalla coda la prima occorrenza del valore value,
	partendo dalla fine della coda ed usando la funzione 
	value_compare per confrontare i valori.
	Restituisce l'elemento rimosso, NULL se non
	è stato trovato all'interno della coda o se i
	parametri non erano validi
*/
void* queueRemoveFirstOccurrance(queue_t *queue, void* value, int (*value_compare)(void*, void*)){
	node_t *tmp;
	
	if( queue == NULL || *value_compare == NULL ) return NULL;
	
	tmp = queue->tail_node;
	while( tmp != NULL ){
		if( value_compare(value, tmp->value) == 0 ){ // elemento trovato
			return queueRemoveByNode(queue, tmp);
		}
		else{
			tmp = tmp->prec_node;
		}
	}
	return NULL;
}

/*
	Restituisce 0 se la coda è vuota, -1 altrimenti
*/
int queueIsEmpty(queue_t *queue){
	if( queue == NULL || queue->head_node == NULL ){
		return 0;
	}
	else{
		return -1;
	}
}

/*
	Restituisce il numero di elementi nella coda
*/
int queueLen(queue_t *queue){
	if( queue != NULL ) return queue->len;
	return 0;
}

/*
	Restituisce una coda contenente N elementi distinti 
	appartenenti al parametro queue, o NULL in caso di errore.
	
	Gli elementi nella coda restituita non sono copie, ma 
	sono gli elementi originali della coda specificata
	come parametro.
	
	Se il numero di elementi richiesti è maggiore di quelli
	effettivamente, vengono restituiti tutti
	gli elementi della coda.
*/
queue_t* queueGetNElems(queue_t *queue, int requested_items){
	queue_t *shallow_queue; // coda in cui salvare gli elementi da restituire
	node_t *tmp; // iteratore della coda
	double remaining_items; // elementi non ancora passati al vaglio dell'iteratore
	int queue_len;
	
	if( queue == NULL || (queue_len = queueLen(queue)) == 0 || (shallow_queue = queueCreate()) == NULL ) return NULL;
	
	if( requested_items > queue_len ) requested_items = queue_len;
	
	tmp = queue->head_node;
	remaining_items = (double) queue_len;
	
	// scorro la coda finchè non ho selezionato tutti
	// gli elementi rimasti
	while( requested_items > 0 ){
		double probability = ((double)requested_items) / remaining_items; // probabilità di scegliere l'elemento attuale in tmp
		double rand_value = (double) rand() / RAND_MAX;
		
		if( rand_value <= probability ){
			if( queueInsert(shallow_queue, tmp->value) == NULL ) return NULL;
			requested_items -= 1;
		}
		
		remaining_items -= 1;
		tmp = tmp->next_node;
	}
	
	return shallow_queue;
}

