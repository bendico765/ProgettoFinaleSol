#if !defined(_CONFIG_PARSER_H)
#define _CONFIG_PARSER_H

#define MAX_FILENAME 100

typedef struct{
	int thread_workers; // numero di thread workers
	int max_memory; // massima quantità memorizzabile in MB
	int max_num_files; // massimo numero di file memorizzabili
	char socket_name[MAX_FILENAME]; // nome del socket
	char log_filename[MAX_FILENAME]; // nome del file di log
}config_t;

int parseConfigFile(char *filename, config_t *server_config);

#endif /* _CONFIG_PARSER_H */
