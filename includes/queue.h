#if !defined(_QUEUE_H)
#define _QUEUE_H

// struct e signature delle funzioni usate per gestire
// una lista doppiamente linkata fifo
typedef struct node_t{
	void* value;
	struct node_t *next_node;
	struct node_t *prec_node;
}node_t;

typedef struct queue_t{
	node_t *head_node;
	node_t *tail_node;
}queue_t;

queue_t* queueCreate();
node_t* queueInsert(queue_t *queue, void *value);
int queueDestroy(queue_t *queue, void (*free_value)(void*));
void* queueRemove(queue_t *queue);
int queueIsEmpty(queue_t *queue);
void* queueRemoveFirstOccurrance(queue_t *queue, void* value, int(*value_compare)(void*, void*));
int queueLen(queue_t *queue);
queue_t* queueGetNElems(queue_t *queue, int N);

#endif
