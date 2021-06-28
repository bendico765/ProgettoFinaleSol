#include "storage.h"
#include <stdlib.h>
#include <string.h>

/*
	Crea lo storage allocando le strutture dati interne
	con i parametri passati.
	Restituisce un puntatore allo storaga in caso di 
	successo, NULL altrimenti
*/
storage_t* storageCreate(int nbuckets, unsigned int (*hash_function)(void*), int (*hash_key_compare)(void*, void*), size_t max_size, size_t max_num_file){
	storage_t *storage;
	
	if( (storage = malloc(sizeof(storage_t))) == NULL ) return NULL;
	storage->file_hash = icl_hash_create(nbuckets, hash_function, hash_key_compare);
	if( storage->file_hash == NULL ){
		free(storage);
		return NULL;
	}
	storage->cache = cacheCreate(max_size, max_num_file);
	if( storage->cache == NULL ){
		icl_hash_destroy(storage->file_hash, NULL, NULL);
		free(storage);
		return NULL;
	}
	return storage;
}

/*
	Restituisce il puntatore al dato associato alla chiave;
	se questo non è stato trovato, restituisce NULL
*/
void* storageFind(storage_t *storage, void *key){
	if( storage != NULL ){
		return icl_hash_find(storage->file_hash, key);
	}
	return NULL;
}

/*
	Prova a inserire l'elemento value (identificato da key) nello storage 
	e restitusce la lista di elementi eventualmente 
	espulsi dopo l'inserimento.
	
	La variabile err viene settata per dare maggior informazioni
	su eventuali errori, e può assumere i seguenti valori:
	 0: inserimento effettuato con successo
	-1: parametri invalidi
	-2: l'elemento non entra nella cache
	-3: errore in qualche allocazione dinamica 
*/
queue_t* storageInsert(storage_t *storage, void *key, void *value, int *err){
	file_t *file;
	queue_t *expelled_files;
	queue_t *tmp_queue;
	file_t *expelled_file;

	if( storage->file_hash == NULL ){
		*err = -1;
		return NULL;
	}
	
	expelled_files = queueCreate();
	if( expelled_files == NULL ){
		*err = -3;
		return NULL;
	}
	
	file = (file_t*) value;
	// inserisco il file in cache espellendo eventuali files
	tmp_queue = cacheInsert(storage->cache, file, err);
	if( *err != 0 ){
		return NULL;
	}
	// rimuovo i files espulsi dallo storage
	while(  (expelled_file = queueRemove(tmp_queue)) != NULL ){
		icl_hash_delete(storage->file_hash, (void*)expelled_file->pathname, NULL, NULL);
		queueInsert(expelled_files, expelled_file);
	}
	// inserisco il nuovo file nello storage
	icl_hash_insert(storage->file_hash, key, value);
	
	queueDestroy(tmp_queue, NULL);
	return expelled_files;
}

/*
	Rimuove l'elemento identificato da key dallo storage, 
	preservando l'ordine della cache ed aggiornando le 
	statistiche interne.
	
	Le funzioni free_key e free_data sono usate per deallocare
	la chiave identificatrice dell'elemento e l'elemento.
	
	Restituisce 0 in caso di successo, altrimenti -1
*/
int storageRemove(storage_t *storage, void *key, void (*free_key)(void*), void (*free_data)(void*)){
	file_t *file_to_remove;
	
	file_to_remove = (file_t*) storageFind(storage, key);
	if( file_to_remove == NULL ) return -1;
	
	// rimozione dell'elemento dalla cache
	if( cacheRemove(storage->cache, file_to_remove) == -1 ) return -1;
	
	// rimozione dell'elemento dallo storage
	if( icl_hash_delete(storage->file_hash, key, NULL, freeFile) == -1 ) return -1;
	return 0;
}

/*
	La variabile err viene settata per dare maggior informazioni
	su eventuali errori, e può assumere i seguenti valori:
	 0: operazione effettuata con successo
	-1: parametri invalidi
	-2: l'elemento modificato non entrerebbe nella cache
	-3: errore nell'allocazione dinamica
*/
queue_t* storageEditFile(storage_t *storage, void *key, char *new_content, size_t file_new_size, int *err){
	file_t *file;
	if( storage == NULL || (file = (file_t*) storageFind(storage, key)) == NULL){
		*err = -1;
		return NULL;
	}
	queue_t *expelled_files;
	node_t *tmp;
	expelled_files = cacheEditFile(storage->cache, file, file_new_size, err);
	if( *err != 0 ) return NULL;
	// rimuovo i files espulsi dallo storage
	tmp = expelled_files->head_node;
	while( tmp != NULL ){
		file_t *expelled_file = (file_t*)tmp->value;
		icl_hash_delete(storage->file_hash, (void*)expelled_file->pathname, NULL, NULL);
		tmp = tmp->next_node;
	}
	// aggiorno la dimensione della cache
	storage->cache->cur_size += (file_new_size - file->size);
	// dealloco le informazioni precedenti
	if(file->content != NULL) free(file->content);
	// aggiorno il file con il nuovo contenuto
	file->content = malloc(file_new_size);
	if( file->content == NULL ){
		*err = -3;
		return NULL;
	}
	memcpy(file->content, new_content, file_new_size);
	file->size = file_new_size;
	
	return expelled_files;
}

/*
	Dealloca tutte le strutture interne dello storage, in particolare
	usando free_key e free_data per deallocare rispettivamente la 
	chiave ed il valore di ogni elemento salvato nello storage
*/
void storageDestroy(storage_t *storage, void (*free_key)(void*), void (*free_data)(void*)){
	if( storage != NULL ){
		cacheDestroy(storage->cache);
		icl_hash_destroy(storage->file_hash, free_key, free_data);
		free(storage);
	}
}

/*
	Restituisce il numero di elementi salvati nello storage
*/
int storageGetNumElements(storage_t *storage){
	if( storage == NULL || storage->cache == NULL ){
		return 0;
	}
	return storage->cache->cur_num_file;
}

/*
	Restituisce la grandezza in bytes dei files salvati
	nello storage
*/
int storageGetSizeElements(storage_t *storage){
	if( storage == NULL || storage->cache == NULL ){
		return 0;
	}
	return storage->cache->cur_size;
}

/*
	Restituisce una coda contenente N elementi dello storage,
	NULL in caso di errore
*/
queue_t* storageGetNElems(storage_t *storage, int N){
	if( storage == NULL || storage->cache == NULL) return NULL;
	return cacheGetNElemsFromCache(storage->cache, N);
}

/*
	Stampa tutto il contenuto dello storage
*/
void storagePrint(storage_t *storage){
	int k;
    icl_entry_t *entry;
    char *key;
    file_t *value;
    icl_hash_t *hash_table = storage->file_hash;
    icl_hash_foreach(hash_table, k, entry, key, value)
	    printf("->%s\n", key);
}
