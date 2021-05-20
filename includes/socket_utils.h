#if !defined(_SOCKET_UTILS_H)
#define _SOCKET_UTILS_H

int initializeServerAndStart(char *sockname, int max_connections);
int initializeClientAndConnect(char *sockname);

#endif
