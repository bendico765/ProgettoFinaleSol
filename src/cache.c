#include "cache.h"
#include <stdlib.h>

#include <string.h>

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
	Prova ad inserire il file nella cache e 
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
	restituisce 0 se i pathname sono diversi, -1 altrimenti
*/
int areFilesDifferent(void *a, void *b){
	file_t *file1 = (file_t*)a;
	file_t *file2 = (file_t*)b;
	if( strcmp(file1->pathname, file2->pathname) != 0 ) return 0;
	return -1;
}

/*
		Sia file un elemento all'interno della cache e file_new_size la 
		nuova dimensione che avrà dopo una modifica, la funzione
		restituisce la lista di files espulsi per far spazio alla 
		modifica del file.
		L'espulsione è fatta preservando l'ordine fifo.
		La variabile err viene settata per dare maggior informazioni
		su eventuali errori, e può assumere i seguenti valori:
		 0: operazione effettuata con successo
		-1: parametri invalidi
		-2: il file modificato non entrerebbe nella cache
		-3: errore nella malloc del nuovo nodo della cache
*/
queue_t* editFileInCache(cache_t *cache, file_t *file, size_t file_new_size, int *err){
	if( cache == NULL || file == NULL ){
		*err = -1; 
		return NULL;
	}
	// se il file modificato è più grande della dimensione totale
	// della cache non ha senso fare ulteriori controlli
	// e termino
	if( file_new_size > cache->max_size ){
		*err = -2;
		return NULL;
	}
	queue_t *expelled_files = queueCreate(); // lista file espulsi
	file_t *expelled_file;
	if( expelled_files == NULL ){ 
		*err = -3;
		return NULL;
	}
	while( (cache->cur_size - file->size + file_new_size ) > cache->max_size && (expelled_file = (file_t*) queueRemoveFirstOccurrance(cache->queue, (void*)file, areFilesDifferent)) != NULL ){
		// aggiorno le stats della cache
		cache->cur_size -= ((file_t*)expelled_file)->size;
		cache->cur_num_file -= 1;
		queueInsert(expelled_files, (void*)expelled_file);
	}
	*err = 0;
	return expelled_files;
}

/*

*/
int removeFromCache(cache_t *cache, file_t *file){
	if( cache == NULL || file == NULL ){
		return -1;
	}
	file_t *removed_file = queueRemoveFirstOccurrance(cache->queue, (void*)file, fileEqual);
	if( removed_file != NULL ){
		cache->cur_size -= removed_file->size;
		cache->cur_num_file -= 1;
		return 0;
	}
	return -1;
}

queue_t* getNElemsFromCache(cache_t *cache, int N){
	if( cache == NULL || cache->queue == NULL ) return NULL;
	return queueGetNElems(cache->queue, N);
}

/*
	Dealloca la cache e la coda FIFO utilizzata da essa.
	Non dealloca i dati effettivi contenuti in cache.
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

