#if !defined(_SIGNAL_HANDLER_H)
#define _SIGNAL_HANDLER_H

#include <signal.h>

struct signal_handler_arg_t{
	sigset_t *set; // maschera di segnali da ascoltare
	int *signal_pipe; // pipe per comunicazione main / signal-handler
} signal_handler_arg_t;

void* threadSignalHandler(void *arg);
int initializeSignalHandler(int signal_handler_pipe[]);

#endif /* _SIGNAL_HANDLER_H */
