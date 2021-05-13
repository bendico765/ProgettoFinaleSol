#if !defined(_CLIENT_UTILS_H)
#define _CLIENT_UTILS_H

#define MAX_DIRNAME_LEN 128
#define MAX_FILENAME_REQUEST 256
#define MAX_SOCKET_NAME 128
#define MAX_ARG_LEN 256

// signature di funzioni per compilare con std=c99
char *strndup(const char *s, size_t n);
int getopt(int argc, char * const argv[], const char *optstring);

// struct e signature delle funzioni usate per gestire
// la coda di richieste del client
struct option{
	char request_opt;
	char *request_content;
	struct option *next_option_ptr;
}option;
typedef struct option* option_ptr;

option_ptr generateNode(char request_opt, char *request_content);
void insertNode(option_ptr *ptr, option_ptr newNode);
void printQueue(option_ptr ptr);
void freeQueue(option_ptr *ptr);

// struct dei parametri del client
typedef struct{
	int socket_fd; // file descriptor del socket
	int p_flag; // flag delle stampe su stdout
	long delay_milliseconds; // ritardo tra le richieste
	char *socket_filename; // nome del socket AF_UNIX
	option_ptr requests_queue; // coda delle richieste del client
} client_params_t; 

void printParams(client_params_t params);

// funzioni generiche usate dal client
int parseOptions(int argc, char *argv[], char optstring[], client_params_t *params);


#endif /* _CLIENT_UTILS_H */
