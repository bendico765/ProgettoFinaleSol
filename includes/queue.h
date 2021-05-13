#if !defined(_QUEUE_H)
#define _QUEUE_H

// struct e signature delle funzioni usate per gestire
// la coda di file descriptors tra server e client
struct node_t{
	int value;
	struct node_t *next_node_ptr;
};
typedef struct node_t node_t;

node_t* generateNode(int value);
void insertNode(node_t **ptr, node_t *new_node);
node_t* removeNode(node_t **ptr);
void printQueue(node_t *ptr);
void freeQueue(node_t **ptr);

#endif
