#include <string.h>
#include <stdlib.h>
#include "client_params.h"
#include "utils.h"

client_params_t* paramsCreate(){
	client_params_t *params;
	
	if( (params = malloc(sizeof(client_params_t))) == NULL ) return NULL;
	
	// inizializzazione parametri client
	params->p_flag = 0;
	params->delay_time = (struct timespec) {0,0};
	memset((void*)params->socket_name, '\0', UNIX_PATH_MAX);
	params->w_args = NULL;
	params->W_args = NULL;
	params->r_args = NULL;
	params->R_args = NULL;
	
	return params;
}

void paramsDestroy(client_params_t *params){
	if( params != NULL ) free(params);
}
