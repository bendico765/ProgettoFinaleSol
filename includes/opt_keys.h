#if !defined(_OPT_KEYS_H)
#define _OPT_KEYS_H

typedef enum{
	// messaggi del client
	OPEN_CONN_OPT = 0, // apertura connessione
	CLOSE_CONN_OPT = 1, // chiusura connessione
	OPEN_FILE_OPT = 2, // apertura file
	CLOSE_FILE_OPT = 3, // chiusura file
	READ_FILE_OPT = 4,
	WRITE_FILE_OPT = 5,
	APPEND_TO_FILE_OPT = 6,
	LOCK_FILE_OPT = 7,
	UNLOCK_FILE_OPT = 8,
	REMOVE_FILE_OPT = 9,
	// messaggi del server
	FILE_ALREADY_EXIT = 10,
	INEXISTENT_FILE = 11,
	FILE_LOCKED = 12,
	FILE_NOT_OPENED = 13,
	CACHE_OVERFLOW = 14,
	SUCCESS = 15
} opt_keys;

#endif /* _OPT_KEYS_H */
