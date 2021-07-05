#if !defined(_FIFO_CACHE_H)
#define _FIFO_CACHE_H

#include <stddef.h>
#include "icl_hash.h"
#include "queue.h"

typedef struct{
	icl_hash_t *elem_hash; // struttura per salvare gli elementi
	queue_t *queue; // coda fifo di elementi nello storage
	size_t max_size; // massima dimensione in bytes della cache
	size_t cur_size; // dimensione attuale della cache
	size_t max_num_elems; // massimo numero di elementi archiviabili
	size_t cur_num_elems; // numero attuale di elementi
}fifo_cache_t;

fifo_cache_t* fifoCacheCreate(int nbuckets, unsigned int (*hashFunction)(void*), int (*hashKeyCompare)(void*, void*), size_t max_size, size_t max_num_elems);
void* fifoCacheFind(fifo_cache_t *cache, void *key);
queue_t* fifoCacheInsert(fifo_cache_t *cache, void *key, void *elem, void* (*getKey)(void*), size_t (*getSize)(void*));
queue_t* fifoCacheEditElem(fifo_cache_t *cache, void *key, void *new_content, size_t elem_new_size, void* (*getKey)(void*), int (*elemEdit)(void*,void*,size_t), size_t (*getSize)(void*), int (*areElemsDifferent)(void*,void*));
void* fifoCacheRemove(fifo_cache_t *cache, void *key, size_t (*getSize)(void*));
queue_t* fifoCacheGetNElemsFromCache(fifo_cache_t *cache, int N);
size_t fifoCacheGetCurrentSize(fifo_cache_t *cache);
size_t fifoCacheGetCurrentNumElements(fifo_cache_t *cache);
void fifoCacheDestroy(fifo_cache_t *cache, void (*freeKey)(void*), void (*freeData)(void*));
void fifoCachePrint(fifo_cache_t *cache, void (*printFunction)(void*,FILE*), FILE *stream);

#endif 
