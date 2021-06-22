#if !defined(_FILE_H)
#define _FILE_H
#include "utils.h" 
#include <stddef.h>

typedef struct{
	char pathname[PATH_LEN_MAX]; 
	char *content;
	size_t size; // dimensione del file in byte
	short freshly_opened; // variabile che indica se il file è stato appena aperto
	short is_open; // variabile che indica se il file è aperto
}file_t;

file_t* generateFile(char pathname[], char *content, size_t size);
int fileEqual(void *file1, void *file2);
void freeFile(void *arg);

#endif /* _FILE_H */
