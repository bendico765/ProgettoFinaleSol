#if !defined(_CLIENT_PARAMS_H)
#define _CLIENT_PARAMS_H

#include "utils.h"
#include <time.h>

typedef struct{
	int p_flag; // flag delle stampe su stdout
	struct timespec delay_time; // ritardo tra le richieste
	char socket_name[UNIX_PATH_MAX]; // nome del socket AF_UNIX
	char *w_args;
	char *W_args;
	char *r_args;
	char *R_args;
} client_params_t;

client_params_t* paramsCreate();
void paramsDestroy(client_params_t *params);

#endif
