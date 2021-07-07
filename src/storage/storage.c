#include "storage.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
	Crea lo storage allocando le strutture dati interne
	con i parametri passati.
	Restituisce un puntatore allo storage in caso di 
	successo, NULL altrimenti (errno impostato)
	
	Parametri:
		-nbuckets: numero di buckets da creare
		-hashFunction: puntatore ad una funzione di hash da usare
		-hashKeyCompare: puntatore ad una funzione che permette di confrontare due chiavi
		-getSize: puntatore ad una funzione che restituisce la dimensione in bytes di un elemento dello storage
		-getKey: puntatore a funzione che, dato un elemento, restituisce una chiave che permetta di identificarlo univocamente 
		-areElemsEqual: puntatore a funzione che permette di determinare se due elementi sono uguali
		-max_size: dimensione massima in bytes dello storage
		-max_num_elems: massimo numero di elementi archiviabili nello storage
		-cache_type: tipo di politica della cache
		
	Valori di errno:
		- EINVAL: parametri invalidi
		- ENOMEM: memoria esaurita
*/

storage_t* storageCreate(int nbuckets, unsigned int (*hashFunction)(void*), int (*hashKeyCompare)(void*, void*), size_t (*getSize)(void*), void* (*getKey)(void*), int (*areElemsEqual)(void*,void*), size_t max_size, size_t max_num_elems, cache_type_t cache_type){
	storage_t *storage;

	// creazione storage
	if( (storage = malloc(sizeof(storage_t))) == NULL ) return NULL;

	// creazione cache 
	switch(cache_type){
		case FIFO: 
			storage->cache = (void*) fifoCacheCreate(nbuckets, hashFunction, hashKeyCompare, max_size, max_num_elems, getSize, getKey, areElemsEqual);
			break;
		case LRU: 
			storage->cache = (void*) lruCacheCreate(nbuckets, hashFunction, hashKeyCompare, max_size, max_num_elems, getSize, getKey, areElemsEqual);
			break;
		default:
			free(storage);
			errno = EINVAL;
			return NULL;
	}
	if( storage->cache == NULL ){
		free(storage);
		return NULL;
	}
	
	storage->cache_type = cache_type;
	
	return storage;
}

/*
	Restituisce il puntatore al dato associato alla chiave;
	se questo non è stato trovato, restituisce NULL.
	
	Errno viene impostato, e può assumere i seguenti valori:
		- EINVAL: parametri invalidi
		- ENOENT: elemento cercato inesistente
*/
void* storageFind(storage_t *storage, void *key){
	if( storage != NULL ){
		switch( storage->cache_type ){
			case FIFO: 
				return fifoCacheFind((fifo_cache_t*)storage->cache, key);
			case LRU:
				return lruCacheFind((lru_cache_t*)storage->cache, key);
			default:
				errno = EINVAL;
				return NULL;
		}
	}
	errno = EINVAL;
	return NULL;
}

/*
	Prova a inserire l'elemento value (identificato da key) nello storage 
	e restitusce la coda di elementi eventualmente espulsi dopo l'inserimento,
	o NULL in caso di errore (errno impostato).
	
	Valori di errno:
		- EINVAL: parametri invalidi
		- EFBIG: elemento troppo grande per entrare in cache
		- ENOMEM: memoria esaurita
*/
queue_t* storageInsert(storage_t *storage, void *key, void *elem){
	if( storage == NULL ){
		errno = EINVAL;
		return NULL;
	}
	
	// inserisco l'elemento in cache espellendo eventuali elementi
	switch( storage->cache_type ){
		case FIFO:
			return fifoCacheInsert((fifo_cache_t*)storage->cache, key, elem);
			break;
		case LRU:
			return lruCacheInsert((lru_cache_t*)storage->cache, key, elem);
			break;
		default:
			errno = EINVAL;
			return NULL;
	}
}

/*
	Rimuove l'elemento identificato da key dallo storage, 
	aggiornando le strutture dati e le statistiche interne.
	
	Le funzioni freeKey e freeData sono usate per deallocare
	la chiave identificatrice dell'elemento e l'elemento. Se 
	non specificate (lasciate a NULL), la chiave/elemento non 
	vengono deallocati.
	
	Restituisce 0 in caso di successo, altrimenti -1 (errno impostato).
	
	Valori di errno:
		- EINVAL: parametri invalidi
		- ENOENT: elemento inesistente
*/
int storageRemove(storage_t *storage, void *key, void (*freeKey)(void*), void (*freeData)(void*)){
	void *content;
	
	if( storage == NULL ){
		errno = EINVAL;
		return -1;
	}
	
	switch( storage->cache_type ){
		case FIFO:
			content = fifoCacheRemove((fifo_cache_t*) storage->cache, key);
			break;
		case LRU:
			content = lruCacheRemove((lru_cache_t*) storage->cache, key);
			break;
		default: // tipo di cache invalido
			errno = EINVAL;
			return -1;
	}
	if( content == NULL ){
		return -1;
	}
	else{
		if( *freeKey != NULL ) (*freeKey)(key);
		if( *freeData != NULL ) (*freeData)(content);
		return 0;
	}
}

/*
	La funzione aggiorna il contenuto dell'elemento nello storage 
	identificato da key, impostando new_content come nuovo contenuto
	e new_size come nuova dimensione dell'elemento. 
	Se la modifica del contenuto dell'elemento causa un overflow
	della cache, gli elementi espulsi vengono eliminati nello storage
	e restituiti all'interno di una coda. 
	In caso di errore viene restituito NULL (errno impostato).
	
	La funzione elemEdit prende 3 parametri: l'elemento da modificare, 
	il nuovo valore e la nuova grandezza dell'elemento, e restituisce
	0 in caso di successo della modifica, -1 altrimenti.
	
	La funzione areElemsDifferent, presi due elementi della cache, 
	restituisce 0 se sono elementi diversi, -1 altrimenti.
	
	Valori di errno:
		- ENOENT: l'elemento identificato da key non esiste nella cache
		- EINVAL: parametri invalidi
		- EFBIG: elemento troppo grande per entrare in cache
		- ENOMEM: memoria esaurita
*/
queue_t* storageEditElem(storage_t *storage, void *key, void *new_content, size_t new_size, int (*elemEdit)(void*,void*,size_t), int (*areElemsDifferent)(void*,void*)){	
	// ricerca dell'elemento nello storage
	if( storage == NULL || storageFind(storage, key) == NULL || *elemEdit == NULL ){
		errno = EINVAL;
		return NULL;
	}
	
	switch( storage->cache_type ){
		case FIFO:
			return fifoCacheEditElem((fifo_cache_t*)storage->cache, key, new_content, new_size, elemEdit, areElemsDifferent);
		case LRU:
			return lruCacheEditElem((lru_cache_t*)storage->cache, key, new_content, new_size, elemEdit);
		default:
			errno = EINVAL;
			return NULL;
	}
}

/*
	Dealloca tutte le strutture interne dello storage, in particolare
	usando freeKey e freeData per deallocare rispettivamente la 
	chiave ed il valore di ogni elemento salvato nello storage.
	
	Se non sono specificate, l'elemento viene rimosso dallo storage, ma
	non vi è deallocazione di memoria.
*/
void storageDestroy(storage_t *storage, void (*freeKey)(void*), void (*freeData)(void*)){
	if( storage != NULL ){
		switch( storage->cache_type ){
			case FIFO:
				fifoCacheDestroy((fifo_cache_t*)storage->cache, freeKey, freeData);
				break;
			case LRU:
				lruCacheDestroy((lru_cache_t*)storage->cache, freeKey, freeData);
				break;
			default:
				break;
		}
		free(storage);
	}
}

/*
	Restituisce il numero di elementi salvati nello storage
*/
size_t storageGetNumElements(storage_t *storage){
	if( storage == NULL ) return 0;
		
	switch( storage->cache_type ){
		case FIFO:
			return fifoCacheGetCurrentNumElements((fifo_cache_t*) storage->cache);
		case LRU:
			return lruCacheGetCurrentNumElements((lru_cache_t*) storage->cache);
		default:
			return 0;
	}
}

/*
	Restituisce la grandezza in bytes degli elementi salvati
	nello storage
*/
size_t storageGetSizeElements(storage_t *storage){
	if( storage == NULL ) return 0;
	
	switch( storage->cache_type ){
		case FIFO:
			return fifoCacheGetCurrentSize((fifo_cache_t*) storage->cache);
		case LRU:
			return lruCacheGetCurrentSize((lru_cache_t*) storage->cache);
		default:
			return 0;
	}
}

/*
	Restituisce una coda contenente N elementi distinti dello storage,
	NULL in caso di errore (errno impostato).
	
	Valori di errno:
		- EINVAL: parametri invalidi
*/
queue_t* storageGetNElems(storage_t *storage, int N){
	if( storage == NULL ){
		errno = EINVAL;
		return NULL;
	}
	
	switch( storage->cache_type ){
		case FIFO:
			return fifoCacheGetNElemsFromCache((fifo_cache_t*) storage->cache, N);
		case LRU:
			return lruCacheGetNElemsFromCache((lru_cache_t*) storage->cache, N);
		default:
			errno = EINVAL;
			return NULL;
	}
}

/*
	La funzione itera tutto il contenuto dello storage ed applica la funzione printFunction
	su ogni singolo elemento.
	
	La funzione printFunction è una funzione che stampa le informazioni relative un elemento
	prendendo come parametri il puntatore all'elemento ed uno stream su cui stampare.
*/
void storagePrint(storage_t *storage, void (*printFunction)(void*,FILE*), FILE *stream){
	if( storage == NULL ) return;
	
	switch( storage->cache_type ){
		case FIFO:
			fifoCachePrint((fifo_cache_t*)storage->cache, printFunction, stream);
			break;
		case LRU:
			lruCachePrint((lru_cache_t*)storage->cache, printFunction, stream);
			break;
		default:
			break;
	}
}
