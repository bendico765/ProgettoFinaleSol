#if !defined(_CONFIG_PARSER_H)
#define _CONFIG_PARSER_H

#include "cache_type.h"
#include "utils.h"

typedef struct{
	int thread_workers; // numero di thread workers
	int max_memory; // massima quantità memorizzabile in bytes
	int max_num_files; // massimo numero di file memorizzabili
	int max_connections; // massimo numero di client che si possono connettere contemporaneamnte
	char socket_name[UNIX_PATH_MAX]; // nome del socket
	char log_filename[PATH_LEN_MAX]; // nome del file di log
	int num_buckets_file; // numero di buckets della hash table dei file nella cache
	cache_type_t cache_type; // tipo di politica di rimpiazzamento della cache
}config_t;

int parseConfigFile(char *filename, config_t *server_config);

#endif /* _CONFIG_PARSER_H */
