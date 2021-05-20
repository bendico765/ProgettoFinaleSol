#if !defined(_MESSAGE_H)
#define _MESSAGE_H

#include "opt_key.h"
#include <stddef.h>

typedef struct{
	opt_key option;
	char filename[]; // 
	int flags;
} message_header_t;

typedef struct{
	size_t size;
	char *content;
} message_content_t;

typedef struct{
	message_header_t header; 
	message_content_t content;
} message_t

#endif /* _MESSAGE_H */
