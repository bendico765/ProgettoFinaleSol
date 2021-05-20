#if !defined(_SIGNAL_HANDLER_H)
#define _SIGNAL_HANDLER_H

#if !defined _GNU_SOURCE
#define _GNU_SOURCE
#endif
#if !defined _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif


#include <signal.h>

struct signal_handler_arg_t{
	sigset_t *set; // maschera impostata dal main
	int *signal_pipe; // pipe per comunicazione main / signal-handler
} signal_handler_arg_t;

void* threadSignalHandler(void *arg);
int initializeSignalHandler(int signal_handler_pipe[]);

#endif /* _SIGNAL_HANDLER_H */
