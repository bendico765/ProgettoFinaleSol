#if !defined(_FILE_H)
#define _FILE_H
//#include "utils.h" 
#include <stddef.h>

typedef struct{
	char pathname[108]; //QUESTA DEVE ESSERE DEFINITA IN UTILS
	char *content;
	size_t size; // dimensione del file in byte
	short blank_file_flag; // ha valore 0 se il file è stato appena creato e mai modificato
}file_t;

file_t* generateFile(char pathname[], char *content, size_t size);
void freeFile(void *arg);

#endif /* _FILE_H */
