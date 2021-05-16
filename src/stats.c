#include "stats.h"
#include <stdio.h>

void printStats(stats_t stats){
	printf("-----\n");
	printf("NUMERO FILES MEMORIZZATI: %d\n", stats.num_files);
	printf("DIMENSIONE FILE STORAGE (MB): %d\n", stats.dim_storage);
	printf("MASSIMO NUMERO DI FILES MEMORIZZATI DURANTE L'ESECUZIONE: %d\n", stats.max_num_files);
	printf("MASSIMA DIMENSIONE DELLO STORAGE RAGGIUNTA DURANTE L'ESECUZIONE: %d\n", stats.max_dim_storage);
	printf("NUMERO DI VOLTE IN CUI L'ALGORITMO DI CACHING È ENTRATO IN FUNZIONE: %d\n", stats.cache_substitutions);
	// lista files memorizzati a fine esecuzione
	printf("-----\n");
}
