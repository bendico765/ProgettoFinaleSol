#include "storage.h"
#include <stdlib.h>

/* 
	Funzioni predefinite usate dallo storage per svolgere certi compiti, 
	sono usate nel caso in cui l'utilizatore non ne specifichi altre. 
*/

size_t storageDefaultGetSize(void* elem){
	return sizeof(elem);
}

void* storageDefaultGetKey(void* elem){
	return elem;
}

int storageDefaultAreElemsEqual(void *e1, void *e2){
	return e1 == e2 ? 0 : -1;
}

/* Funzioni dello storage */

/*
	Crea lo storage allocando le strutture dati interne
	con i parametri passati.
	Restituisce un puntatore allo storage in caso di 
	successo, NULL altrimenti
	
	Parametri:
		-nbuckets: numero di buckets da create
		-hashFunction: puntatore ad una funzione di hash da usare
		-hashKeyCompare: puntatore ad una funzione che permette di confrontare due chiavi
		-getSize: puntatore ad una funzione che restituisce la dimensione in bytes di un elemento dello storage
		-getKey: puntatore a funzione che, dato un elemento, restituisce una chiave che permetta di identificarlo univocamente 
		-areElemsEqual: puntatore a funzione che permette di determinare se due elementi sono uguali
		-max_size: dimensione massima in bytes dello storage
		-max_num_elems: massimo numero di elementi archiviabili nello storage
*/
storage_t* storageCreate(int nbuckets, unsigned int (*hashFunction)(void*), int (*hashKeyCompare)(void*, void*), size_t (*getSize)(void*), void* (*getKey)(void*), int (*areElemsEqual)(void*,void*), size_t max_size, size_t max_num_elems){
	storage_t *storage;
	
	// creazione tabella hash
	if( (storage = malloc(sizeof(storage_t))) == NULL ) return NULL;
	storage->elem_hash = icl_hash_create(nbuckets, hashFunction, hashKeyCompare);
	if( storage->elem_hash == NULL ){
		free(storage);
		return NULL;
	}
	
	// creazione cache FIFO
	storage->cache = cacheCreate(max_size, max_num_elems);
	if( storage->cache == NULL ){
		icl_hash_destroy(storage->elem_hash, NULL, NULL);
		free(storage);
		return NULL;
	}
	
	// impostazione funzioni
	storage->getSize = *getSize == NULL ? storageDefaultGetSize : getSize;
	storage->getKey = *getKey == NULL ?  storageDefaultGetKey : getKey;
	storage->areElemsEqual = *areElemsEqual == NULL ? storageDefaultAreElemsEqual : areElemsEqual;
	
	return storage;
}

/*
	Restituisce il puntatore al dato associato alla chiave;
	se questo non è stato trovato, restituisce NULL
*/
void* storageFind(storage_t *storage, void *key){
	if( storage != NULL ){
		return icl_hash_find(storage->elem_hash, key);
	}
	return NULL;
}

/*
	Prova a inserire l'elemento value (identificato da key) nello storage 
	e restitusce la coda di elementi eventualmente 
	espulsi dopo l'inserimento.
	
	La variabile err viene settata per dare maggior informazioni
	su eventuali errori, e può assumere i seguenti valori:
	 0: inserimento effettuato con successo
	-1: parametri invalidi
	-2: l'elemento non entra nella cache
	-3: errore in qualche allocazione dinamica 
*/
queue_t* storageInsert(storage_t *storage, void *key, void *elem, int *err){
	queue_t *expelled_elements;
	queue_t *tmp_queue;
	void *expelled_elem; 

	if( storage == NULL || storage->elem_hash == NULL ){
		*err = -1;
		return NULL;
	}
	
	expelled_elements = queueCreate();
	if( expelled_elements == NULL ){
		*err = -3;
		return NULL;
	}
	
	// inserisco l'elemento in cache espellendo eventuali elementi
	tmp_queue = cacheInsert(storage->cache, elem, storage->getSize, err);
	if( *err != 0 ){
		return NULL;
	}
	// rimuovo gli elementi espulsi dallo storage
	while(  (expelled_elem = queueRemove(tmp_queue)) != NULL ){
		void *expelled_elem_key = (* storage->getKey)(expelled_elem);
		icl_hash_delete(storage->elem_hash, expelled_elem_key, NULL, NULL);
		queueInsert(expelled_elements, expelled_elem);
	}
	// inserisco il nuovo elemento nello storage
	icl_hash_insert(storage->elem_hash, key, elem);
	
	queueDestroy(tmp_queue, NULL);
	return expelled_elements;
}

/*
	Rimuove l'elemento identificato da key dallo storage, 
	preservando l'ordine della cache ed aggiornando le 
	statistiche interne.
	
	Le funzioni freeKey e freeData sono usate per deallocare
	la chiave identificatrice dell'elemento e l'elemento. Se 
	non specificate (lasciate a NULL), la chiave/elemento non 
	vengono deallocati.
	
	Restituisce 0 in caso di successo, altrimenti -1
*/
int storageRemove(storage_t *storage, void *key, void (*freeKey)(void*), void (*freeData)(void*)){
	void *element_to_remove;
	size_t element_size;
	
	if( storage == NULL || key == NULL ) return -1;
	
	element_to_remove = storageFind(storage, key);
	if( element_to_remove == NULL ) return -1;
	
	// ottenimento della dimensione dell'elemento
	element_size = (* storage->getSize)(element_to_remove);
	
	// rimozione dell'elemento dalla cache
	if( cacheRemove(storage->cache, element_to_remove, element_size, storage->areElemsEqual) == -1 ) return -1;
	
	// rimozione dell'elemento dallo storage
	if( icl_hash_delete(storage->elem_hash, key, freeKey, freeData) == -1 ) return -1;
	return 0;
}

/*
	La funzione aggiorna il contenuto dell'elemento nello storage 
	identificato da key, impostando new_content come nuovo contenuto
	e new_size come nuova dimensione dell'elemento. 
	Se la modifica del contenuto dell'elemento causa un overflow
	della cache, gli elementi espulsi vengono eliminati nello storage
	e restituiti all'interno di una coda.
	
	La funzione elemEdit prende 3 parametri: l'elemento da modificare, 
	il nuovo valore e la nuova grandezza dell'elemento, e restituisce
	0 in caso di successo della modifica, -1 altrimenti.
	
	La funzione elemsAreDifferent, dati due elementi, restituisce 0 se
	questi sono diversi, o un valore diverso altrimenti.
	

	La variabile err viene settata per dare maggior informazioni
	su eventuali errori, e può assumere i seguenti valori:
	 0: operazione effettuata con successo
	-1: parametri invalidi
	-2: l'elemento modificato sforerebbe i limiti della cache
	-3: errore nell'allocazione dinamica
*/
queue_t* storageEditElem(storage_t *storage, void *key, void *new_content, size_t new_size, int (*elemEdit)(void*,void*,size_t), int (*elemsAreDifferent)(void*,void*), int *err){
	void *element_to_edit;
	size_t elem_size;
	queue_t *expelled_elements;
	queue_t *tmp_queue;
	void *expelled_elem;
	
	// ricerca dell'elemento nello storage
	if( storage == NULL || (element_to_edit = storageFind(storage, key)) == NULL || *elemEdit == NULL || *elemsAreDifferent == NULL ){
		*err = -1;
		return NULL;
	}
	
	elem_size = (* storage->getSize)(element_to_edit);
	
	expelled_elements = queueCreate();
	if( expelled_elements == NULL ){
		*err = -3;
		return NULL;
	}
	
	tmp_queue = cacheEditElem(storage->cache, element_to_edit, new_size, storage->getSize, elemsAreDifferent, err);
	if( *err != 0 ){
		queueDestroy(expelled_elements, NULL);
		return NULL;
	}
	
	// rimuovo gli elementi espulsi dallo storage
	while( (expelled_elem = queueRemove(tmp_queue)) != NULL ){
		void *expelled_element_key = (* storage->getKey)(expelled_elem);
		icl_hash_delete(storage->elem_hash, expelled_element_key, NULL, NULL);
		queueInsert(expelled_elements, expelled_elem);
	}
	// aggiorno la dimensione della cache
	storage->cache->cur_size += (new_size - elem_size);
	
	// aggiorno l'elemento con il nuovo contenuto
	if( (*elemEdit)(element_to_edit, new_content, new_size) == -1 ){
		*err = -3;
		return NULL;
	}

	queueDestroy(tmp_queue, NULL);
	return expelled_elements;
}

/*
	Dealloca tutte le strutture interne dello storage, in particolare
	usando freeKey e freeData per deallocare rispettivamente la 
	chiave ed il valore di ogni elemento salvato nello storage.
	
	Se non sono specificate, l'elemento viene rimosso dallo storage, ma
	non vi è deallocazione di memoria.
*/
void storageDestroy(storage_t *storage, void (*freeKey)(void*), void (*freeData)(void*)){
	if( storage != NULL ){
		cacheDestroy(storage->cache);
		icl_hash_destroy(storage->elem_hash, freeKey, freeData);
		free(storage);
	}
}

/*
	Restituisce il numero di elementi salvati nello storage
*/
size_t storageGetNumElements(storage_t *storage){
	if( storage == NULL ){
		return 0;
	}
	return cacheGetCurrentNumElements(storage->cache);
}

/*
	Restituisce la grandezza in bytes degli elementi salvati
	nello storage
*/
size_t storageGetSizeElements(storage_t *storage){
	if( storage == NULL ){
		return 0;
	}
	return cacheGetCurrentSize(storage->cache);
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
void storagePrint(storage_t *storage, void (*printFunction)(void*,FILE*), FILE *stream){
	if( storage != NULL && storage->elem_hash != NULL && *printFunction != NULL && stream != NULL ){
		int k;
		icl_entry_t *entry;
		void *key;
		void *value;
		icl_hash_t *hash_table = storage->elem_hash;
		icl_hash_foreach(hash_table, k, entry, key, value)
			(*printFunction)(value, stream);
	}
}
