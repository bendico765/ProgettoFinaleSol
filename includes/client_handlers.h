#if !defined(_CLIENT_HANDLERS_H)
#define _CLIENT_HANDLERS_H

#include "client_params.h"

void hOptionHandler(char *program_name);
int fOptionHandler(client_params_t *params, char *optarg);
int cOptionHandler(client_params_t *params, char *optarg);
int wOptionHandler(client_params_t *params, char *optarg, char *expelled_files_dirname);
int WOptionHandler(client_params_t *params, char *optarg, char *expelled_files_dirname);
int rOptionHandler(client_params_t *params, char *optarg, char *expelled_files_dirname);
int ROptionHandler(client_params_t *params, char *optarg, char *expelled_files_dirname);
int tOptionHandler(client_params_t *params, char *optarg);

#endif
