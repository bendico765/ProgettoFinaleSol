#include "cache.h"
#include <stdlib.h>
#include <string.h>

/*
	Crea una cache FIFO di dimensione massima in bytes 
	pari a max_size e massimo numero di elementi pari a 
	max_num_elems.
	Inizializza a zero i contatori e la coda di elementi,
	e restituisce la cache allocata, NULL in caso di errore.
*/
cache_t* cacheCreate(size_t max_size, size_t max_num_elems){
	cache_t *new_cache;
	
	if( max_size <= 0 || max_num_elems <= 0 ) return NULL;
	
	new_cache = malloc(sizeof(cache_t));
	if( new_cache == NULL ){ return NULL; }
	
	new_cache->queue = queueCreate();
	if( new_cache->queue == NULL ){ 
		free(new_cache); 
		return NULL; 
	}
	
	new_cache->max_size = max_size;
	new_cache->cur_size = 0;
	new_cache->max_num_elems = max_num_elems;
	new_cache->cur_num_elems = 0;
	return new_cache;
}

/*
	Prova ad inserire un elemento nella cache e restitusce 
	una coda contenente gli elementi eventualmente espulsi 
	dopo l'inserimento. 
	
	La variabile err viene settata per dare maggior informazioni
	su eventuali errori, e può assumere i seguenti valori:
	 0: inserimento effettuato con successo
	-1: parametri invalidi
	-2: l'elemento non entra nella cache
	-3: errore nella malloc del nuovo nodo della cache
*/
queue_t* cacheInsert(cache_t *cache, void *elem, size_t (*get_size)(void*), int *err){
	queue_t *expelled_elements; // lista elementi espulsi
	size_t elem_size; // grandezza in bytes dell'elemento da inserire
	
	if( cache == NULL || elem == NULL || *get_size == NULL){
		*err = -1; 
		return NULL;
	}
	
	elem_size = (*get_size)(elem);
	
	// se l'elemento è più grande della dimensione totale
	// della cache non ha senso fare ulteriori controlli
	// e termino
	if( elem_size > cache->max_size ){
		*err = -2;
		return NULL;
	}
	
	expelled_elements = queueCreate(); 
	if( expelled_elements == NULL ){ 
		*err = -3;
		return NULL;
	}
	
	// controllo l'eventuale sforamento del limite numero elementi
	if( cache->cur_num_elems == cache->max_num_elems ){
		// rimuovo un elemento dalla cache
		void *expelled_element = queueRemove(cache->queue); 
		// aggiorno le statistiche della cache
		cache->cur_size -= (*get_size)(expelled_element);
		cache->cur_num_elems -= 1;
		// inserisco l'elemento espulso nella coda
		queueInsert(expelled_elements, expelled_element);
	}
	// l'elemento da inserire potrebbe ancora essere troppo grande
	// per la cache, quindi devo continuare ad espellere elementi
	while( (cache->cur_size + elem_size) > cache->max_size ){
		// rimuovo un elemento dalla cache
		void *expelled_element = queueRemove(cache->queue); 
		// aggiorno le statistiche della cache
		cache->cur_size -= (*get_size)(expelled_element);
		cache->cur_num_elems -= 1;
		// inserisco l'elemento espulso nella coda
		queueInsert(expelled_elements, expelled_element);
	}
	// il nuovo elemento può ora entrare nella cache
	queueInsert( cache->queue, elem );
	cache->cur_num_elems += 1;
	cache->cur_size += elem_size;
	*err = 0;
	return expelled_elements;
}

/*
	Sia elem un elemento all'interno della cache e elem_new_size la 
	nuova dimensione che avrà dopo una modifica, la funzione
	restituisce una coda di elementi espulsi per far spazio alla 
	modifica.
	
	L'espulsione è fatta preservando l'ordine fifo.
	
	La variabile err viene settata per dare maggior informazioni
	su eventuali errori, e può assumere i seguenti valori:
	 0: operazione effettuata con successo
	-1: parametri invalidi
	-2: l'elemento modificato non entrerebbe nella cache
	-3: errore nell'allocazione dinamica di memoria
*/
queue_t* cacheEditElem(cache_t *cache, void *elem, size_t elem_new_size, size_t (*get_size)(void*), int (*are_elems_different)(void*,void*), int *err){
	queue_t *expelled_elements; // coda elementi espulsi
	void *expelled_element;
	size_t element_actual_size;

	if( cache == NULL || elem == NULL || *get_size == NULL || *are_elems_different == NULL){
		*err = -1; 
		return NULL;
	}
	
	element_actual_size = (*get_size)(elem);
	
	// se l'elemento modificato è più grande della dimensione totale
	// della cache non ha senso fare ulteriori controlli
	// e termino
	if( elem_new_size > cache->max_size ){
		*err = -2;
		return NULL;
	}
	
	expelled_elements = queueCreate(); 
	if( expelled_elements == NULL ){ 
		*err = -3;
		return NULL;
	}
	
	// continuo a rimuovere elementi finchè la modifica dell'elemento comporterebbe overflow
	while( (cache->cur_size - element_actual_size + elem_new_size ) > cache->max_size && (expelled_element =  queueRemoveFirstOccurrance(cache->queue, elem, are_elems_different)) != NULL ){
		// aggiorno le stats della cache
		cache->cur_size -=  (*get_size)(expelled_element);
		cache->cur_num_elems -= 1;
		queueInsert(expelled_elements, expelled_element);
	}
	*err = 0;
	return expelled_elements;
}

/*
	Rimuove l'elemento dalla cache (aggiornando le statistiche interne).
	
	Restituisce 0 se l'elemento è stato rimosso, -1 se non è stato 
	trovato. 
*/
int cacheRemove(cache_t *cache, void *elem, size_t element_size, int (*are_elems_equal)(void*,void*)){
	void *removed_element;
	if( cache == NULL || elem == NULL || *are_elems_equal == NULL){
		return -1;
	}
	
	removed_element = queueRemoveFirstOccurrance(cache->queue, elem, are_elems_equal);
	if( removed_element != NULL ){
		cache->cur_size -= element_size;
		cache->cur_num_elems -= 1;
		return 0;
	}
	return -1;
}

/*
	Restituisce una coda contenente N elementi dalla cache,
	NULL in caso di errore
*/
queue_t* cacheGetNElemsFromCache(cache_t *cache, int N){
	if( cache == NULL || cache->queue == NULL ) return NULL;
	return queueGetNElems(cache->queue, N);
}

/*
	Restituisce la dimensione in bytes degli elementi
	attualmente salvati in cache
*/
size_t cacheGetCurrentSize(cache_t *cache){
	if( cache == NULL ) return 0;
	return cache->cur_size;
}

/*
	Restituisce il numero di elementi attualmente salvati in cache
*/
size_t cacheGetCurrentNumElements(cache_t *cache){
	if( cache == NULL ) return 0;
	return cache->cur_num_elems;
}

/*
	Dealloca la cache e la coda FIFO utilizzata da essa.
	Non dealloca i dati effettivi contenuti in cache.
*/
void cacheDestroy(cache_t *cache){
	if( cache != NULL ){
		queueDestroy(cache->queue, NULL);
		free(cache);
	}
}

