#if !defined(_STATS_H)
#define _STATS_H

typedef struct{
	int num_files; // numero files memorizzati
	int max_num_files; // massimo numero files memorizzati dall'inizio del programma
	int dim_storage; // dimensione complessiva dei files memorizzata nello storage
	int max_dim_storage; // massima dimensione complessiva raggiunta durante l'esecuzione
	// lista di file memorizzati
	int cache_substitutions; // numero di volte in cui l'algoritmo di cache è stato eseguito
} stats_t;

void printStats(stats_t stats);

#endif 
