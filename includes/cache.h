#if !defined(_CACHE_H)
#define _CACHE_H

#include "queue.h"
#include "file.h"

typedef struct{
	queue_t *queue; // coda dei file nello storage
	size_t max_size; // massima dimensione in bytes della cache
	size_t cur_size; // dimensione attuale della cache
	size_t max_num_file; // massimo numero di file archiviabili
	size_t cur_num_file; // numero attuale di file
}cache_t;

cache_t* cacheCreate(size_t max_size, size_t max_num_file);
queue_t* cacheInsert(cache_t *cache, file_t *file, int *err);
queue_t* cacheEditFile(cache_t *cache, file_t *file, size_t file_new_size, int *err);
int cacheRemove(cache_t *cache, file_t *file);
queue_t* cacheGetNElemsFromCache(cache_t *cache, int N);
void cacheDestroy(cache_t *cache);

#endif /* _CACHE_H */
