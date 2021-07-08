#include "fifo_cache.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* 
	Funzioni predefinite usate dalla cache per svolgere certi compiti, 
	sono usate nel caso in cui l'utilizzatore non ne specifichi altre. 
*/

size_t fifoCacheDefaultGetSize(void* elem){
	return sizeof(elem);
}

void* fifoCacheDefaultGetKey(void* elem){
	return elem;
}

int fifoCacheDefaultAreElemsEqual(void *e1, void *e2){
	return e1 == e2 ? 0 : -1;
}

/*
	Crea una cache FIFO di dimensione massima in bytes 
	pari a max_size e massimo numero di elementi pari a 
	max_num_elems.
	Inizializza i contatori e le strutture dati necessarie,
	e restituisce la cache allocata, NULL in caso di errore 
	(impostando errno).
	
	Parametri:
		-nbuckets: numero di buckets da creare
		-hashFunction: puntatore ad una funzione di hash da usare
		-hashKeyCompare: puntatore ad una funzione che permette di confrontare due chiavi
		-max_size: dimensione massima in bytes dello storage
		-max_num_elems: massimo numero di elementi archiviabili nello storage
		-getSize: puntatore ad una funzione che restituisce la dimensione in bytes di un elemento dello storage
		-getKey: puntatore a funzione che, dato un elemento, restituisce una chiave che permetta di identificarlo univocamente 
		-areElemsEqual: puntatore a funzione che permette di determinare se due elementi sono uguali
		
	Valori di errno:
		- EINVAL: parametri invalidi
		- ENOMEM: memoria esaurita
*/
fifo_cache_t* fifoCacheCreate(int nbuckets, unsigned int (*hashFunction)(void*), int (*hashKeyCompare)(void*, void*), size_t max_size, size_t max_num_elems, size_t (*getSize)(void*), void* (*getKey)(void*), int (*areElemsEqual)(void*,void*)){
	fifo_cache_t *new_cache;
	
	if( max_size <= 0 || max_num_elems <= 0 ){
		errno = EINVAL;
		return NULL;
	}
	
	new_cache = malloc(sizeof(fifo_cache_t));
	if( new_cache == NULL )	return NULL;
	
	// creazione hash table
	new_cache->elem_hash = icl_hash_create(nbuckets, hashFunction, hashKeyCompare);
	if( new_cache->elem_hash == NULL ){
		free(new_cache);
		return NULL;
	}
	
	// creazione coda fifo
	new_cache->queue = queueCreate();
	if( new_cache->queue == NULL ){ 
		icl_hash_destroy(new_cache->elem_hash, NULL, NULL);
		free(new_cache); 
		return NULL; 
	}
	
	// impostazione funzioni
	new_cache->getSize = *getSize == NULL ? fifoCacheDefaultGetSize : getSize;
	new_cache->getKey = *getKey == NULL ?  fifoCacheDefaultGetKey : getKey;
	new_cache->areElemsEqual = *areElemsEqual == NULL ? fifoCacheDefaultAreElemsEqual : areElemsEqual;
	
	// impostazione contatori
	new_cache->max_size = max_size;
	new_cache->cur_size = 0;
	new_cache->max_num_elems = max_num_elems;
	new_cache->cur_num_elems = 0;
	return new_cache;
}

/*
	Restituisce il puntatore al dato associato alla chiave;
	se questo non è stato trovato, restituisce NULL 
	(impostando errno).
	
	Valori di errno:
		- EINVAL: parametri invalidi
		- ENOENT: elemento cercato inesistente
*/
void* fifoCacheFind(fifo_cache_t *cache, void *key){
	node_t *elem_node;
	
	if( cache != NULL ){
		elem_node = (node_t*)icl_hash_find(cache->elem_hash, key);
		if( elem_node == NULL ){ // elemento non trovato
			errno = ENOENT;
			return NULL;
		}
		else return elem_node->value;
	}
	else{
		errno = EINVAL;
		return NULL;
	}
}

/*
	Prova ad inserire un elemento nella cache e restitusce 
	una coda contenente gli elementi eventualmente espulsi 
	dopo l'inserimento, o NULL in caso di errore (errno impostato)
	
	La funzione getSize, preso un elemento nella cache, ne restituisce
	la dimensione in bytes.
	
	La funzione getKey, dato un elemento nella cache, restituisce una 
	chiave che permetta di identificarlo univocamente 
	
	Valori di errno:
		- EINVAL: parametri invalidi
		- EFBIG: elemento troppo grande per entrare in cache
		- ENOMEM: memoria esaurita
*/
queue_t* fifoCacheInsert(fifo_cache_t *cache, void *key, void *elem){
	node_t *new_node; // nuovo nodo contenente l'elemento da inserire
	queue_t *expelled_elements; // lista elementi espulsi
	size_t elem_size; // grandezza in bytes dell'elemento da inserire
	void *expelled_element; // elemento rimosso dalla cache in seguito ad overflow
	
	if( cache == NULL || elem == NULL){
		errno = EINVAL;
		return NULL;
	}
	
	elem_size = (*cache->getSize)(elem);
	
	// se l'elemento è più grande della dimensione totale
	// della cache non ha senso fare ulteriori controlli
	// e termino
	if( elem_size > cache->max_size ){
		errno = EFBIG;
		return NULL;
	}
	
	expelled_elements = queueCreate(); 
	if( expelled_elements == NULL ){ 
		return NULL;
	}
	
	// controllo l'eventuale sforamento del limite numero elementi
	// o del limite sulla dimensione massima
	while( (cache->cur_num_elems == cache->max_num_elems) || (cache->cur_size + elem_size) > cache->max_size ){
		// rimuovo un elemento dalla cache
		expelled_element = queueRemove(cache->queue); 
		// aggiorno le statistiche della cache
		cache->cur_size -= (*cache->getSize)(expelled_element);
		cache->cur_num_elems -= 1;
		// rimuovo l'elemento espulso dalla tabella hash
		icl_hash_delete(cache->elem_hash, (*cache->getKey)(expelled_element), NULL, NULL);
		// inserisco l'elemento espulso nella coda
		queueInsert(expelled_elements, expelled_element);
	}
	
	// il nuovo elemento può ora entrare nella cache
	
	// inserimento nella coda
	new_node = queueInsert( cache->queue, elem );
	if( new_node == NULL ){
		return NULL;
	}
	// inserimento nella tabella hash
	if( icl_hash_insert(cache->elem_hash, key, new_node) == NULL){
		errno = ENOMEM;
		return NULL;
	}
	
	cache->cur_num_elems += 1;
	cache->cur_size += elem_size;
	return expelled_elements;
}

/*
	La funzione aggiorna il contenuto dell'elemento nello storage 
	identificato da key, impostando new_content come nuovo contenuto
	e new_size come nuova dimensione dell'elemento. 
	Se la modifica del contenuto dell'elemento causa un overflow
	della cache, gli elementi espulsi vengono eliminati nello storage
	e restituiti all'interno di una coda.
	
	La funzione elemEdit prende 3 parametri: l'elemento da modificare, 
	il nuovo valore e la nuova grandezza dell'elemento, e restituisce
	0 in caso di successo della modifica, -1 altrimenti.
	
	La funzione areElemsDifferent, presi due elementi della cache, 
	restituisce 0 se sono elementi diversi, -1 altrimenti.
	
	Valori di errno:
		- ENOENT: l'elemento identificato da key non esiste
		- EINVAL: parametri invalidi
		- EFBIG: elemento troppo grande per entrare in cache
		- ENOMEM: memoria esaurita
*/
queue_t* fifoCacheEditElem(fifo_cache_t *cache, void *key, void *new_content, size_t elem_new_size, int (*elemEdit)(void*,void*,size_t), int (*areElemsDifferent)(void*,void*)){
	queue_t *expelled_elements; // coda elementi espulsi
	node_t *elem_node;
	void *expelled_element;
	void *element_to_edit;
	size_t element_actual_size;

	if( cache == NULL || key == NULL || *elemEdit == NULL || *areElemsDifferent == NULL){
		errno = EINVAL;
		return NULL;
	}
	
	// cerco l'elemento identificato dalla chiave nella cache
	// e calcolo la dimensione
	elem_node = (node_t*) icl_hash_find(cache->elem_hash, key);
	if( elem_node == NULL ){
		errno = ENOENT;
		return NULL;
	}
	if( (element_to_edit = elem_node->value) == NULL ){
		errno = EINVAL;
		return NULL;
	}
	element_actual_size = (*cache->getSize)(element_to_edit);
	
	// se l'elemento modificato è più grande della dimensione totale
	// della cache non ha senso fare ulteriori controlli
	// e termino
	if( elem_new_size > cache->max_size ){
		errno = EFBIG;
		return NULL;
	}
	
	expelled_elements = queueCreate(); 
	if( expelled_elements == NULL ){ 
		return NULL;
	}
	
	// continuo a rimuovere elementi finchè la modifica dell'elemento comporterebbe overflow
	while( (cache->cur_size - element_actual_size + elem_new_size ) > cache->max_size && (expelled_element =  queueRemoveFirstOccurrance(cache->queue, element_to_edit, areElemsDifferent)) != NULL ){
		// aggiorno le stats della cache
		cache->cur_size -=  (*cache->getSize)(expelled_element);
		cache->cur_num_elems -= 1;
		queueInsert(expelled_elements, expelled_element);
		// rimuovo l'elemento dallo storage
		icl_hash_delete(cache->elem_hash, (*cache->getKey)(expelled_element), NULL, NULL);
	}
	
	// aggiorno la dimensione della cache
	cache->cur_size += (elem_new_size - element_actual_size);
	
	// aggiorno l'elemento con il nuovo contenuto
	if( (*elemEdit)(element_to_edit, new_content, elem_new_size) == -1 ){
		return NULL;
	}
	
	return expelled_elements;
}

/*
	Rimuove l'elemento identificato da key dalla cache 
	(aggiornando le statistiche interne).
	
	Restituisce il contenuto dell'elemento rimosso, NULL in 
	caso di errore (errno impostato).
	
	Valori di errno:
		- EINVAL: parametri invalidi
		- ENOENT: elemento inesistente
	
*/
void* fifoCacheRemove(fifo_cache_t *cache, void *key){
	node_t *node_to_remove;
	void *node_content;
	
	if( cache == NULL || key == NULL){
		errno = EINVAL;
		return NULL;
	}
	
	// cerco l'elemento nella cache
	node_to_remove = (node_t*)icl_hash_find(cache->elem_hash, key);
	if( node_to_remove != NULL ){ // elemento trovato
		// lo rimuovo dalla coda
		node_content = queueRemoveByNode(cache->queue, node_to_remove);
		// lo rimuovo dalla tabella hash
		icl_hash_delete(cache->elem_hash, key, NULL, NULL);
		// aggiorno i contatori interni
		cache->cur_size -= (*cache->getSize)(node_content);
		cache->cur_num_elems -= 1;
		
		return node_content;
	}
	else{ // elemento inesistente
		errno = ENOENT;
		return NULL;
	}
	
}

/*
	Restituisce una coda contenente N elementi distinti dalla cache,
	NULL in caso di errore (errno impostato).
	
	Valori di errno:
		- EINVAL: parametri invalidi
*/
queue_t* fifoCacheGetNElemsFromCache(fifo_cache_t *cache, int N){
	if( cache == NULL ) return NULL;
	return queueGetNElems(cache->queue, N);
}

/*
	Restituisce la dimensione in bytes degli elementi
	attualmente salvati in cache
*/
size_t fifoCacheGetCurrentSize(fifo_cache_t *cache){
	if( cache == NULL ) return 0;
	return cache->cur_size;
}

/*
	Restituisce il numero di elementi attualmente salvati in cache
*/
size_t fifoCacheGetCurrentNumElements(fifo_cache_t *cache){
	if( cache == NULL ) return 0;
	return cache->cur_num_elems;
}

/*
	Dealloca la cache e la coda FIFO utilizzata da essa.
*/
void fifoCacheDestroy(fifo_cache_t *cache, void (*freeKey)(void*), void (*freeData)(void*)){
	if( cache != NULL ){
		icl_hash_destroy(cache->elem_hash, freeKey, NULL);
		queueDestroy(cache->queue, freeData);
		free(cache);
	}
}

/*
	La funzione itera tutto il contenuto della cache ed applica la funzione printFunction
	su ogni singolo elemento.
	
	La funzione printFunction è una funzione che stampa le informazioni relative un elemento
	prendendo come parametri il puntatore all'elemento ed uno stream su cui stampare.
*/
void fifoCachePrint(fifo_cache_t *cache, void (*printFunction)(void*,FILE*), FILE *stream){
	if( cache != NULL ) queuePrint(cache->queue, printFunction, stream);
}

