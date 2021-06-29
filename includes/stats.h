#if !defined(_STATS_H)
#define _STATS_H

typedef struct{
	size_t num_files; // numero files memorizzati
	size_t max_num_files; // massimo numero files memorizzati dall'inizio del programma
	size_t dim_storage; // dimensione complessiva dei files memorizzata nello storage
	size_t max_dim_storage; // massima dimensione complessiva raggiunta durante l'esecuzione
	int cache_substitutions; // numero di volte in cui l'algoritmo di cache è stato eseguito
} stats_t;

#endif 
