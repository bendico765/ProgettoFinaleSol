#ifndef _STORAGE_H
#define _STORAGE_H

#include "icl_hash.h"
#include "cache.h"
#include "file.h"
#include "queue.h"

typedef struct{
	icl_hash_t *file_hash; // struttura per salvare i files
	cache_t *cache; // struttura per la gestione della politica di caching
}storage_t;

storage_t* storageCreate(int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*), size_t max_size, size_t max_num_file);
void* storageFind(storage_t *storage, void *key);
queue_t* storageInsert(storage_t *storage, void *key, void* value, int *err);
int storageRemove(storage_t *storage, void *key, void (*free_key)(void*), void (*free_data)(void*));
queue_t* storageEditFile(storage_t *storage, void *key, char *new_content, size_t file_new_size, int *err);
void storageDestroy(storage_t *storage, void (*free_key)(void*), void (*free_data)(void*));
int storageGetNumElements(storage_t *storage);
int storageGetSizeElements(storage_t *storage);
queue_t* storageGetNElems(storage_t *storage, int N);
void storagePrint(storage_t *storage, void (*printFunction)(void*,FILE*), FILE *stream);

#endif
