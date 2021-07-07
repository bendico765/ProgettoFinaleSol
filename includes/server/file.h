#if !defined(_FILE_H)
#define _FILE_H
#include "utils.h" 
#include <stddef.h>
#include <stdio.h>

typedef struct{
	char pathname[PATH_LEN_MAX]; 
	void *content;
	size_t size; // dimensione del file in byte
	int freshly_opened; // variabile che indica se il file è stato appena aperto ed ancora non modificato
	int is_open; // variabile che indica se il file è aperto
}file_t;

file_t* fileGenerate(char pathname[], void *content, size_t size);
int fileEqual(void *file1, void *file2);
int fileEdit(void *file, void *new_content, size_t new_size);
void filePrintInfo(void *file, FILE *stream);
size_t fileGetSize(void *file);
int fileAreDifferent(void *file1, void *file2);
void* fileGetPathname(void *file);
void fileFree(void *arg);

#endif /* _FILE_H */
