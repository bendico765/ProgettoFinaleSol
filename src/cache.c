#include "cache.h"
#include <stdlib.h>

/*
	Crea una cache FIFO di dimensione massima in bytes 
	pari a max_size e massimo numero di file pari a 
	max_num_file.
	Inizializza a zero i contatori e la coda di files,
	e restituisce la cache allocata, NULL in caso di errore.
*/
cache_t* createCache(size_t max_size, size_t max_num_file){
	if( max_size <= 0 || max_num_file <= 0 ){ return NULL;}
	cache_t *new_cache = malloc(sizeof(cache_t));
	if( new_cache == NULL ){ return NULL; }
	new_cache->queue = queueCreate();
	if( new_cache->queue == NULL ){ 
		free(new_cache); 
		return NULL; 
	}
	new_cache->max_size = max_size;
	new_cache->cur_size = 0;
	new_cache->max_num_file = max_num_file;
	new_cache->cur_num_file = 0;
	return new_cache;
}

/*
	Prova ad inserire il nodo contenente un file nella cache e 
	restitusce la lista di file eventualmente 
	espulsi dopo l'inserimento.
	La variabile err viene settata per dare maggior informazioni
	su eventuali errori, e può assumere i seguenti valori:
	 0: inserimento effettuato con successo
	-1: parametri invalidi
	-2: il file non entra nella cache
	-3: errore nella malloc del nuovo nodo della cache
*/
queue_t* insertFileIntoCache(cache_t *cache, file_t *file, int *err){
	if( cache == NULL || file == NULL ){
		*err = -1; 
		return NULL;
	}
	// se il file è più grande della dimensione totale
	// della cache non ha senso fare ulteriori controlli
	// e termino
	if( file->size > cache->max_size ){
		*err = -2;
		return NULL;
	}
	queue_t *expelled_files = queueCreate(); // lista file espulsi
	if( expelled_files == NULL ){ 
		*err = -3;
		return NULL;
	}
	// sforamento limite numero files
	if( cache->cur_num_file == cache->max_num_file ){
		// rimuovo un file dalla cache
		void *expelled_file = queueRemove(cache->queue); 
		// aggiorno le statistiche della cache
		cache->cur_size -= ((file_t*)expelled_file)->size;
		cache->cur_num_file -= 1;
		// inserisco il file espulso nella lista
		queueInsert(expelled_files, expelled_file);
	}
	// il file da inserire potrebbe ancora essere troppo grande
	// per la cache, quindi devo continuare ad espellere files
	while( (cache->cur_size + file->size) > cache->max_size ){
		// rimuovo un file dalla cache
		void *expelled_file = queueRemove(cache->queue); 
		// aggiorno le statistiche della cache
		cache->cur_size -= ((file_t*)expelled_file)->size;
		cache->cur_num_file -= 1;
		// inserisco il file espulso nella lista
		queueInsert(expelled_files, expelled_file);
	}
	// il file può ora entrare nella cache
	queueInsert( cache->queue, (void*)file );
	cache->cur_num_file += 1;
	cache->cur_size += file->size;
	*err = 0;
	return expelled_files;
}

/*
	Dealloca la cache e la coda FIFO utilizzata da essa
*/
void destroyCache(cache_t *cache){
	if( cache == NULL ){
		return;
	}
	else{
		queueDestroy(cache->queue, NULL);
		free(cache);
	}
}

