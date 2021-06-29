#if !defined(_CACHE_H)
#define _CACHE_H

#include <stddef.h>
#include "queue.h"

typedef struct{
	queue_t *queue; // coda di elementi nello storage
	size_t max_size; // massima dimensione in bytes della cache
	size_t cur_size; // dimensione attuale della cache
	size_t max_num_elems; // massimo numero di elementi archiviabili
	size_t cur_num_elems; // numero attuale di elementi
}cache_t;

cache_t* cacheCreate(size_t max_size, size_t max_num_elems);
queue_t* cacheInsert(cache_t *cache, void *elem, size_t (*get_size)(void*), int *err);
queue_t* cacheEditElem(cache_t *cache, void *elem, size_t elem_new_size, size_t (*get_size)(void*), int (*are_elems_different)(void*,void*), int *err);
int cacheRemove(cache_t *cache, void *elem, size_t element_size, int (*are_elems_equal)(void*,void*));
queue_t* cacheGetNElemsFromCache(cache_t *cache, int N);
size_t cacheGetCurrentSize(cache_t *cache);
size_t cacheGetCurrentNumElements(cache_t *cache);
void cacheDestroy(cache_t *cache);

#endif /* _CACHE_H */
