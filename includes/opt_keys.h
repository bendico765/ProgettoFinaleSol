#if !defined(_OPT_KEYS_H)
#define _OPT_KEYS_H

typedef enum{
	// messaggi del client
	OPEN_CONN_OPT = 0, // apertura connessione
	CLOSE_CONN_OPT = 1, // chiusura connessione
	OPEN_FILE_OPT = 2, // apertura file
	CLOSE_FILE_OPT = 3, // chiusura file
	READ_FILE_OPT = 4, // lettura file
	WRITE_FILE_OPT = 5, // scrittura file
	APPEND_TO_FILE_OPT = 6, // append a file
	LOCK_FILE_OPT = 7, // lock file
	UNLOCK_FILE_OPT = 8, // unlock file
	REMOVE_FILE_OPT = 9, // eliminazione file
	// messaggi del server
	FILE_ALREADY_EXIT = 10, // file già esistente (in caso di creazione con O_CREAT)
	INEXISTENT_FILE = 11, // file non esistente
	FILE_LOCKED = 12, // file locked
	FILE_NOT_OPENED = 13, // tentativo di scrittura su file non aperto
	CACHE_SUBSTITUTION = 14, // arrivo del file espulso dalla cache
	CACHE_FULL = 15, // cache piena e tutti i file sono lockati
	SUCCESS = 16 // operazione terminata con successo
} opt_keys;

#endif /* _OPT_KEYS_H */
