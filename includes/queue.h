#if !defined(_QUEUE_H)
#define _QUEUE_H

// struct e signature delle funzioni usate per gestire
// una coda fifo
typedef struct node_t{
	void* value;
	struct node_t *next_node;
}node_t;

typedef struct queue_t{
	node_t *head_node;
	node_t *tail_node;
}queue_t;

queue_t* queueCreate();
node_t* queueInsert(queue_t *queue, void *value);
void queueDestroy(queue_t *queue, void (*free_value)(void*));
void* queueRemove(queue_t *queue);

#endif
