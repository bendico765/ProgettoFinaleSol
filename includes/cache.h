#if !defined(_CACHE_H)
#define _CACHE_H

#include "queue.h"
#include "file.h"

typedef struct{
	queue_t *queue; // coda dei file nello storage
	size_t max_size; // massima dimensione in bytes dello storage
	size_t cur_size; // dimensione attuale dello storage
	size_t max_num_file; // massimo numero di file archiviabili nello storage
	size_t cur_num_file; // numero attuale di file nello storage
}cache_t;

cache_t* createCache(size_t max_size, size_t max_num_file);
queue_t* insertFileIntoCache(cache_t *cache, file_t *file, int *err);
void destroyCache(cache_t *cache);

#endif /* _CACHE_H */
