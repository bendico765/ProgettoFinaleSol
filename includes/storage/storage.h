#ifndef _STORAGE_H
#define _STORAGE_H

#include "fifo_cache.h"
#include "lru_cache.h"
#include "queue.h"
#include "cache_type.h"

#include <stdio.h>

typedef struct{
	void *cache;
	cache_type_t cache_type; // tipo di politica di caching
}storage_t;

storage_t* storageCreate(int nbuckets, unsigned int (*hashFunction)(void*), int (*hashKeyCompare)(void*, void*), size_t (*getSize)(void*), void* (*getKey)(void*), int (*areElemsEqual)(void*,void*), size_t max_size, size_t max_num_elems, cache_type_t cache_type);
void* storageFind(storage_t *storage, void *key);
queue_t* storageInsert(storage_t *storage, void *key, void* value);
int storageRemove(storage_t *storage, void *key, void (*freeKey)(void*), void (*freeData)(void*));
queue_t* storageEditElem(storage_t *storage, void *key, void *new_content, size_t new_size, int (*elemEdit)(void*,void*,size_t), int (*areElemsDifferent)(void*,void*));
void storageDestroy(storage_t *storage, void (*freeKey)(void*), void (*freeData)(void*));
size_t storageGetNumElements(storage_t *storage);
size_t storageGetSizeElements(storage_t *storage);
queue_t* storageGetNElems(storage_t *storage, int N);
void storagePrint(storage_t *storage, void (*printFunction)(void*,FILE*), FILE *stream);

#endif
