#if !defined(_OPT_KEYS_H)
#define _OPT_KEYS_H

typedef enum{
	// messaggi del client
	OPEN_FILE_OPT = 0, // apertura file
	CLOSE_FILE_OPT = 1, // chiusura file
	READ_FILE_OPT = 2, // lettura file
	READ_N_FILE_OPT = 3, // lettura N files
	WRITE_FILE_OPT = 4, // scrittura file
	APPEND_TO_FILE_OPT = 5, // append a file
	REMOVE_FILE_OPT = 6, // eliminazione file
	// messaggi del server
	FILE_ALREADY_EXIST = 7, // file già esistente (in caso di creazione con O_CREAT)
	INEXISTENT_FILE = 8, // file non esistente
	PATHNAME_TOO_LONG = 9, // pathname supera la lunghezza massima
	FILE_NOT_OPENED = 10, // tentativo di scrittura su file non aperto
	CACHE_SUBSTITUTION = 11, // arrivo del file espulso dalla cache
	SUCCESS = 12 // operazione terminata con successo
} opt_keys;

#endif /* _OPT_KEYS_H */
