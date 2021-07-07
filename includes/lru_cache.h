#if !defined(_LRU_CACHE_H)
#define _LRU_CACHE_H

#include <stddef.h>
#include "icl_hash.h"
#include "queue.h"

typedef struct{
	icl_hash_t *elem_hash; // struttura per salvare gli elementi
	queue_t *queue; // coda per tenere traccia dell'ordine degli elementi
	size_t max_size; // massima dimensione in bytes della cache
	size_t cur_size; // dimensione attuale della cache
	size_t max_num_elems; // massimo numero di elementi archiviabili
	size_t cur_num_elems; // numero attuale di elementi
	size_t (*getSize)(void*); // funzione per calcolare la dimensione in bytes di un elemento nella cache
	void* (*getKey)(void*); // funzione che, dato un elemento nello cache, restituisce una chiave che permetta di identificarlo univocamente
	int (*areElemsEqual)(void*,void*); // funzione per determinare se due elementi sono uguali
}lru_cache_t;

lru_cache_t* lruCacheCreate(int nbuckets, unsigned int (*hashFunction)(void*), int (*hashKeyCompare)(void*, void*), size_t max_size, size_t max_num_elems, size_t (*getSize)(void*), void* (*getKey)(void*), int (*areElemsEqual)(void*,void*));
void* lruCacheFind(lru_cache_t *cache, void *key);
queue_t* lruCacheInsert(lru_cache_t *cache, void *key, void *elem);
queue_t* lruCacheEditElem(lru_cache_t *cache, void *key, void *new_content, size_t elem_new_size, int (*elemEdit)(void*,void*,size_t));
void* lruCacheRemove(lru_cache_t *cache, void *key);
queue_t* lruCacheGetNElemsFromCache(lru_cache_t *cache, int N);
size_t lruCacheGetCurrentSize(lru_cache_t *cache);
size_t lruCacheGetCurrentNumElements(lru_cache_t *cache);
void lruCacheDestroy(lru_cache_t *cache, void (*freeKey)(void*), void (*freeData)(void*));
void lruCachePrint(lru_cache_t *cache, void (*printFunction)(void*,FILE*), FILE *stream);

#endif 
