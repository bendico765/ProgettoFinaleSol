#if !defined(_MESSAGE_H)
#define _MESSAGE_H

#include "opt_keys.h"
#include "utils.h"
#include <stddef.h>

typedef struct{
	opt_keys option;
	char filename[PATH_LEN_MAX]; 
	int flags;
} message_header_t;

typedef struct{
	size_t size;
	char *content;
} message_content_t;

int sendMessageHeader(int socket_fd, opt_keys option, char *filename, int flags);
int receiveMessageHeader(int socket_fd, message_header_t *hdr);
int sendMessageContent(int socket_fd, size_t size, char* content);
message_content_t* receiveMessageContent(int socket_fd);

#endif /* _MESSAGE_H */
