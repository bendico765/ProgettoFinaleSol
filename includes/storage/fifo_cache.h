#if !defined(_FIFO_CACHE_H)
#define _FIFO_CACHE_H

#include <stddef.h>
#include "icl_hash.h"
#include "queue.h"

/*
	Spiegazione sommaria sul funzionamento della cache:
	
	Quando un elemento viene inserito nella cache, viene prima di tutto
	inserito nella coda queue (causando eventuali overflow ed espulsioni);
	l'elemento inserito si troverà all'interno di un nodo nella coda (di tipo
	node_t).
	La tabella hash elem_hash associa ad ogni chiave il puntatotore
	al nodo della coda in cui tale elemento è contenuto.
	
	Cosi facendo, eventuali operazioni di modifica/rimozione di un elemento
	non necessitano di scorrere tutta la coda alla ricerca del nodo contenente
	l'elemento in questione, ma possono essere fatte in tempo costante.
*/

typedef struct{
	icl_hash_t *elem_hash; // struttura per salvare gli elementi
	queue_t *queue; // coda fifo di elementi nello storage
	size_t max_size; // massima dimensione in bytes della cache
	size_t cur_size; // dimensione attuale della cache
	size_t max_num_elems; // massimo numero di elementi archiviabili
	size_t cur_num_elems; // numero attuale di elementi
	size_t (*getSize)(void*); // funzione per calcolare la dimensione in bytes di un elemento nella cache
	void* (*getKey)(void*); // funzione che, dato un elemento nello cache, restituisce una chiave che permetta di identificarlo univocamente
	int (*areElemsEqual)(void*,void*); // funzione per determinare se due elementi sono uguali
}fifo_cache_t;

fifo_cache_t* fifoCacheCreate(int nbuckets, unsigned int (*hashFunction)(void*), int (*hashKeyCompare)(void*, void*), size_t max_size, size_t max_num_elems, size_t (*getSize)(void*), void* (*getKey)(void*), int (*areElemsEqual)(void*,void*));
void* fifoCacheFind(fifo_cache_t *cache, void *key);
queue_t* fifoCacheInsert(fifo_cache_t *cache, void *key, void *elem);
queue_t* fifoCacheEditElem(fifo_cache_t *cache, void *key, void *new_content, size_t elem_new_size, int (*elemEdit)(void*,void*,size_t), int (*areElemsDifferent)(void*,void*));
void* fifoCacheRemove(fifo_cache_t *cache, void *key);
queue_t* fifoCacheGetNElemsFromCache(fifo_cache_t *cache, int N);
size_t fifoCacheGetCurrentSize(fifo_cache_t *cache);
size_t fifoCacheGetCurrentNumElements(fifo_cache_t *cache);
void fifoCacheDestroy(fifo_cache_t *cache, void (*freeKey)(void*), void (*freeData)(void*));
void fifoCachePrint(fifo_cache_t *cache, void (*printFunction)(void*,FILE*), FILE *stream);

#endif 
