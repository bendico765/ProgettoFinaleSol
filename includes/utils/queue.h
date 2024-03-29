#if !defined(_QUEUE_H)
#define _QUEUE_H

#include <stdio.h>

/*  
	Coda doppiamente linkata con politica fifo
	(inserimenti in testa e rimozioni in coda).
	
	Tuttavia vi sono alcune operazioni che violano 
	questa politica, come ad esempio queueRemoveFirstOcurance,
	che permette di rimuovere la prima occorrenza di un elemento
	(partendo dal fondo della coda)
*/
typedef struct node_t{
	void* value;
	struct node_t *next_node;
	struct node_t *prec_node;
}node_t;

typedef struct queue_t{
	node_t *head_node;
	node_t *tail_node;
	int len;
}queue_t;

queue_t* queueCreate();
node_t* queueInsert(queue_t *queue, void *value);
int queueReinsert(queue_t *queue, node_t *node);
int queueDestroy(queue_t *queue, void (*freeValue)(void*));
void* queueRemove(queue_t *queue);
void* queueRemoveByNode(queue_t *queue, node_t *node);
void* queueRemoveFirstOccurrance(queue_t *queue, void* value, int(*valueCompare)(void*, void*));
int queueIsEmpty(queue_t *queue);
int queueLen(queue_t *queue);
queue_t* queueGetNElems(queue_t *queue, int N); // richiede di chiamare srand() per generare una sequenza casuale
void queuePrint(queue_t *queue, void (*printFunction)(void*,FILE*), FILE *stream);

#endif
