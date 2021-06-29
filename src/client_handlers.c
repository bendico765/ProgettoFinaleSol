#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "client_handlers.h"
#include "client_params.h"
#include "queue.h"
#include "api.h"

/*  
	Funzioni di utilità usate dalle implementazioni
	degli handlers delle opzioni
*/

/*
	La funzione fa il parsing degli argomenti in optarg
	usando ',' come carattere delimitatore e restituisce 
	una coda contenente i singoli argomenti.
	In caso di errore viene restituito NULL.
*/
queue_t* parseOptionArg(char *optarg){
	queue_t *queue;
	char *temp;
	char *dup;
	char *copy;
	
	if( (copy = strdup(optarg)) == NULL ) return NULL;
	if( (queue = queueCreate()) == NULL ) return NULL;
	
	temp = strtok(copy, ",");
	while( temp != NULL ){
		if( ( dup = strdup(temp) ) == NULL ){
			queueDestroy(queue, free);
			return NULL;
		}
		queueInsert(queue, (void*)dup);
		temp = strtok(NULL, ",");
	}
	if( copy != NULL ) free(copy);
	return queue;
}

/*
	La funzione prende num_files files dalla directory source_dir (visitando 
	ricorsivamente anche le sottocartelle) e ne salva il percorso assoluto
	in pathnames_queue.
	Se num_files è uguale a -1, vengono presi tutti i files.
	
	Restituisce 0 in caso di successo, -1 in caso di errore (errno settato);
*/
int getPathnamesFromDir(char *source_dir, int *num_files, queue_t *pathnames_queue){
	DIR *dir;
	struct dirent *file;
	char *relative_pathname;
	char *absolute_pathname;
	
	// apertura della cartella sorgente dei files
	dir = opendir(source_dir);
	if( dir == NULL ) return -1;
	
	errno = 0;
	// lettura di tutti i files (e directory) della cartella
	while( (file = readdir(dir)) != NULL && *num_files != 0){
		// andiamo avanti se si tratta delle directory ./ e ../
		if(strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0){
			continue;
        }
        // si ricava il pathname relativo del file
        relative_pathname = (char*) calloc(strlen(source_dir) + strlen(file->d_name) + 2, sizeof(char));
        if( relative_pathname == NULL ) return -1;
        strcat(relative_pathname,source_dir);
        strcat(relative_pathname,"/");
        strcat(relative_pathname,file->d_name);
        
        if( file->d_type != DT_DIR ){ // non è una directory
        	// si ricava il pathname assoluto del file
        	absolute_pathname = relativeToAbsolutePath(relative_pathname);
        	if( absolute_pathname == NULL ) return -1;
        	// il pathname assoluto viene inserito nella coda
        	if( queueInsert(pathnames_queue, (void*)absolute_pathname) == NULL ){
        		return -1;
        	}
        	if( *num_files != -1 ) *num_files -= 1;
        }
        else{ // è una directory da visitare ricorsivamente
        	if( getPathnamesFromDir(relative_pathname, num_files, pathnames_queue) == -1 ) return -1;
        }
        free(relative_pathname);
	}
	if( errno != 0 ) return -1;
	
	free(dir);
	return 0;
}

/*
	La funzione scrive i files (identificati dai loro percorsi assoluti in files_list) 
	sul server (identificato da socket_name) e salva gli eventuali files espulsi nella 
	directory expelled_files_dirname. Le richieste di scrittura su server si intervallano 
	di un tempo pari a delay_time. Se print_flag è diverso da 0, sono abilitate le stampe 
	delle operazioni sullo standard output.
	
	Nel caso in cui sia stato impossibile scrivere un file, la funzione non fallisce, ma 
	prova a scrivere gli altri file rimasti.
	
	La funzione restituisce -1 in caso di errore, 0 in caso di successo.
*/
int writeFilesToServer(queue_t *files_list, char *expelled_files_dirname, struct timespec *delay_time, int print_flag){
	char *absolute_pathname;
	struct stat statbuf;
	
	// scriviamo i files sul server
	while( (absolute_pathname = (char*) queueRemove(files_list)) != NULL){
		// ritardo tra due richieste consecutive
		nanosleep(delay_time, NULL);
		// creazione del nuovo file
		if( openFile(absolute_pathname, 1) == -1 ){
			fprintf(stderr, "Impossibile creare il file %s: %s\n", absolute_pathname, strerror(errno));
			free(absolute_pathname);
			continue;
		}
		// file creato correttamente
		if( writeFile(absolute_pathname, expelled_files_dirname) == -1 ){
			fprintf(stderr, "Errore durante la scrittura del file %s: %s\n", absolute_pathname, strerror(errno));
			free(absolute_pathname);
			continue;
		}
		if( print_flag != 0){
			if( stat(absolute_pathname, &statbuf) == -1 ){
				fprintf(stdout, "Il file %s è stato scritto con successo\n", absolute_pathname);
			}
			else{
				fprintf(stdout, "Il file %s [%ld bytes] [%.3f MB] è stato scritto con successo\n", absolute_pathname, (size_t)statbuf.st_size, ((float)statbuf.st_size)/1000000);
			}
		}
		free(absolute_pathname);
	}
	
	return 0;
}

/*
	La funzione legge i files (identificati dai loro percorsi assoluti in files_list) 
	dal server (identificato da socket_name) e salva i files letti nella (eventuale)
	directory dest_dirname. Se dest_dirname è NULL, i files letti sono buttati via.
	Le richieste di lettura al server si intervallano di un tempo pari a delay_time. 
	Se print_flag è diverso da 0, sono abilitate le stampe delle operazioni sullo 
	standard output.
	
	Nel caso in cui sia stato impossibile leggere un file (o si sia verificato un errore
	con la sua scrittura su disco), la funzione non fallisce, ma prova a leggere gli altri 
	file rimasti.
	
	La funzione restituisce -1 in caso di errore, 0 in caso di successo.
*/
int readFileFromServer(queue_t *files_list, char *dest_dirname, struct timespec *delay_time, int print_flag){
	char *pathname; // percorso del file da leggere

	// lettura dei files da server
	while( (pathname = (char*) queueRemove(files_list)) != NULL){
		void *file_content; // contenuto del file letto da server
		size_t file_size; // grandezza (in bytes) del file letto da server
		// ritardo tra due richieste consecutive
		nanosleep(delay_time, NULL);
		
		if( readFile(pathname, &file_content, &file_size) == -1 ){
			fprintf(stderr, "Errore durante la lettura del file %s: %s\n", pathname, strerror(errno));
			free(pathname);
			if( file_content != NULL ) free(file_content);
			continue;
		}
		if( print_flag != 0 ){
			fprintf(stdout, "Il file %s [%ld bytes] [%.3f MB] è stato scaricato con successo\n", pathname, file_size, ((float)file_size)/1000000);
		}
		// Scriviamo (eventualmente) il file nella directory specificata
		if( dest_dirname != NULL ){
			FILE *file; // file in cui scrivere il contenuto letto da server
			char *filename; // nome del file
			char *new_path; // nuovo percorso del file
			
			// estrapolo il nome del file scaricato 
			filename = absolutePathToFilename(pathname);
			if( filename == NULL ){
				fprintf(stderr, "Errore durante la scrittura su disco del file %s: %s\n", pathname, strerror(errno));
				free(pathname);
				free(file_content);
				continue;
			}
					
			// ricaviamo il percorso relativo del file da scrivere
			new_path = calloc(strlen(dest_dirname) + strlen(filename) + 2, sizeof(char));
			if( new_path == NULL ){
				fprintf(stderr, "Errore durante la scrittura su disco del file %s: %s\n", pathname, strerror(errno));
				free(filename);
				free(pathname);
				free(file_content);
				continue;
			}
			strcat(new_path, dest_dirname);
			strcat(new_path, "/");
			strcat(new_path, filename);
			
			// scrivo il file su disco
			file = fopen(new_path, "wb");
			if( file == NULL){
				fprintf(stderr, "Errore durante la scrittura su disco del file %s: %s\n", pathname, strerror(errno));
				free(filename);
				free(pathname);
				free(file_content);
				free(new_path);
				continue;
			}
			fwrite(file_content, file_size, 1, file);
			if( print_flag != 0 ) fprintf(stdout, "Il file %s è stato scritto in %s con successo\n", pathname, dest_dirname);
			free(filename);
			free(new_path);
			fclose(file);
		}
		free(file_content);
		free(pathname);
	}

	return 0;
}

/* Handlers delle opzioni del client */

void hOptionHandler(char *program_name){
	char *options;
	options = "-f filename -w dirname[,n=0] -W file1[,file2] -D dirname -r file1[,file2] -R [n=0] -d dirname -t time -c file1[,file2] -p";
	fprintf(stdout, "%s %s\n", program_name, options);
}

int fOptionHandler(client_params_t *params, char *optarg){
	if( strnlen(optarg, UNIX_PATH_MAX+1) < UNIX_PATH_MAX+1 ){ // controllo che il socket name non sia troppo lungo
		if( params->socket_name[0] == '\0' )
			strncpy(params->socket_name, optarg, UNIX_PATH_MAX);
		else{
			fprintf(stderr, "Un nome per il socket è già stato definito\n");
			return -1;
		}
	}
	else{
		fprintf(stderr, "Il socket name supera la lunghezza consentita (%d caratteri)\n", UNIX_PATH_MAX);
		return -1;
	}
	if(openConnection(params->socket_name, 0, (struct timespec){0,0}) == -1){
		perror("Errore nella connessione con il server");
		return -1;
	}
	return 0;
}

/*
	Implementazione dell'opzione -c del client, prova a rimuovere
	da server (identificato da params->socket_name) i files
	specificati nell'argomento optarg dell'opzione.
	
	Nel caso in cui sia stato impossibile cancellare un file, 
	la funzione non fallisce, ma prova a cancellare gli altri 
	file rimasti.
	
	Restituisce 0 in caso di successo, -1 altrimenti.
*/
int cOptionHandler(client_params_t *params, char *optarg){
	queue_t *args_option;
	char *pathname;
	
	// parsing degli argomenti dell'opzione
	args_option = parseOptionArg(optarg);
	if( args_option == NULL ){
		perror("Errore durante l'elaborazione dei parametri");
		return -1;
	}

	// rimozione dei files richiesti
	while( (pathname = (char*) queueRemove(args_option)) != NULL){
		if( removeFile(pathname) != 0 ){
			fprintf(stderr, "Errore durante la rimozione del file %s: %s\n", pathname, strerror(errno));
		}
		else{
			if( params->p_flag != 0 ) fprintf(stdout, "File %s rimosso con successo\n", pathname);
		}
		free(pathname);
	}
	
	queueDestroy(args_option, free);
	return 0;
}

/*
	Implementazione dell'opzione -w del client, prova a scrivere
	su server (identificato da params->socket_name) i files
	nella cartella specificata nell'argomento optarg dell'opzione,
	salvando in expelled_files_dirname i files eventualmente espulsi.
	
	Nel caso in cui sia stato impossibile scrivere un file, 
	la funzione non fallisce, ma prova a scrivere gli altri 
	file rimasti.
	
	Restituisce 0 in caso di successo, -1 altrimenti.
*/
int wOptionHandler(client_params_t *params, char *optarg, char *expelled_files_dirname){
	queue_t *args_option; // argomenti di -w parsati
	queue_t *to_send_files; // lista con i pathnames dei files da mandare
	int num_args; // numero di argomenti di -w
	int num_files; // numero di files da mandare al server
	char *source_directory; // directory sorgente da cui prelevare i files da mandare
	char *tmp;
	DIR *dir;
	
	to_send_files = queueCreate();
	if( to_send_files == NULL ){
		perror("Errore con i files nella directory sorgente");
		return -1;
	}
	
	// verifica l'esistenza della eventuale directory
	// in cui salvare i files espulsi
	dir = NULL;
	if( expelled_files_dirname != NULL && (dir = opendir(expelled_files_dirname)) == NULL ){
		perror("Errore con la cartella di destinazione");
		return -1;
	}
	
	// faccio il parsing degli argomenti dell'opzione -w
	args_option = parseOptionArg(optarg);
	if( args_option == NULL ){
		perror("Errore durante l'elaborazione dei parametri");
		return -1;
	}
	
	// verifica di quali argomenti sono stati specificati
	num_args = queueLen(args_option);
	switch(num_args){
		case 1: // numero files non specificato
			source_directory = (char*) queueRemove(args_option);
			num_files = -1;
			break;
		case 2: // numero files specificato
			source_directory = (char*) queueRemove(args_option);
			tmp = (char*) queueRemove(args_option);
			if( isIntNumber(tmp, &num_files) != 0 ){
				// numero files non è un numero valido
				perror("Errore con il secondo parametro di -w");
				return -1;
			}
			free(tmp);
			if( num_files == 0 ) num_files = -1;
			break;
		default:
			fprintf(stderr, "Parametri non validi");
			return -1;
			break;
	}
	
	// salvataggio in to_send_files i pathanames dei files
	// da mandare al server
	if( getPathnamesFromDir(source_directory, &num_files, to_send_files) != 0 ){
		perror("Errore durante la lettura dei files da mandare");
		return -1;
	}
	
	// scrittura dei files su server
	
	if( writeFilesToServer(to_send_files, expelled_files_dirname, &params->delay_time, params->p_flag) == -1 ) return -1;
	
	if( dir != NULL ) free(dir);
	free(source_directory);
	queueDestroy(args_option, free);
	queueDestroy(to_send_files, free);
	return 0;
}

/*
	Implementazione dell'opzione -W del client, prova a scrivere
	su server (identificato da params->socket_name) i files
	specificati nell'argomento optarg dell'opzione, salvando 
	in expelled_files_dirname i files eventualmente espulsi.
	
	Nel caso in cui sia stato impossibile scrivere un file, 
	la funzione non fallisce, ma prova a scrivere gli altri 
	file rimasti.
	
	Restituisce 0 in caso di successo, -1 altrimenti.
*/
int WOptionHandler(client_params_t *params, char *optarg, char *expelled_files_dirname){
	queue_t *args_option;
	queue_t *pathnames_list;
	char *filename;
	char *absolute_path;
	DIR *dir;
	
	// verifica l'esistenza della eventuale directory
	dir = NULL;
	if( expelled_files_dirname != NULL && (dir = opendir(expelled_files_dirname)) == NULL ){
		perror("Errore con la cartella di destinazione");
		return -1;
	}
	// parsing degli argomenti dell'opzione
	args_option = parseOptionArg(optarg);
	if( args_option == NULL ){
		perror("Errore durante l'elaborazione dei parametri");
		return -1;
	}
	
	// trasformazione dei pathnames relativi in assoluti
	pathnames_list = queueCreate();
	if( pathnames_list == NULL ){
		perror("Errore durante l'esecuzione del comando");
		return -1;
	}
	
	while( (filename = (char*)queueRemove(args_option)) != NULL ){
    	absolute_path = relativeToAbsolutePath(filename);
		if( absolute_path == NULL ){
			fprintf(stderr, "Impossibile risolvere il percorso del file %s: %s\n", filename, strerror(errno));
		}
		else{ 
			if( queueInsert(pathnames_list, (void*)absolute_path) == NULL ){
				perror("Errore durante l'esecuzione del comando");
				return -1;
			}
		}
		free(filename);
	}

	// scrivo i files sul server
	if( writeFilesToServer(pathnames_list, expelled_files_dirname, &params->delay_time, params->p_flag) == -1 ) return -1;
	
	if( dir != NULL ) free(dir);
	queueDestroy(pathnames_list, free);
	queueDestroy(args_option, free);
	return 0;
}

/*
	Implementazione dell'opzione -r del client, prova a leggere
	da server (identificato da params->socket_name) i files
	specificati nell'argomento optarg dell'opzione, salvando 
	in files_dirname i files ricevuti.
	Se files_dirname è NULL, i file letti sono buttati via.
	
	Nel caso in cui sia stato impossibile leggere un file, 
	la funzione non fallisce, ma prova a leggere gli altri 
	file rimasti.
	
	Restituisce 0 in caso di successo, -1 altrimenti.
*/
int rOptionHandler(client_params_t *params, char *optarg, char *files_dirname){
	queue_t *args_option;
	DIR *dir;
	
	// verifica l'esistenza della eventuale directory 
	dir = NULL;
	if( files_dirname != NULL && (dir = opendir(files_dirname)) == NULL ){
		perror("Errore con la cartella di destinazione");
		return -1;
	}
	
	// parsing degli argomenti dell'opzione
	args_option = parseOptionArg(optarg);
	if( args_option == NULL ){
		perror("Errore durante l'elaborazione dei parametri");
		return -1;
	}
	
	// lettura del contenuto dei files e (se specificato) scrittura 
	// nella directory
	if( readFileFromServer(args_option, files_dirname, &params->delay_time, params->p_flag) == -1){
		perror("Errore durante la lettura dei files");
	}
	
	if( dir != NULL ) free(dir);
	queueDestroy(args_option, free);
	return 0;
}

/*
	Implementazione dell'opzione -R del client, prova a leggere
	da server (identificato da params->socket_name) il numero
	di files specificato nell'argomento optarg dell'opzione, 
	salvando in files_dirname i files ricevuti.
	Se files_dirname è NULL, i file letti sono buttati via.
	
	Nel caso in cui sia stato impossibile leggere un file, 
	la funzione non fallisce, ma prova a leggere gli altri 
	file rimasti.
	
	Restituisce 0 in caso di successo, -1 altrimenti.
*/
int ROptionHandler(client_params_t *params, char *optarg, char *files_dirname){
	queue_t *args_option;
	DIR *dir;
	int num_files; // numero files da leggere
	int read_files; // numero di files effettivamente ricevuti dal server
	
	// verifica l'esistenza della eventuale directory
	dir = NULL;
	if( files_dirname != NULL && (dir = opendir(files_dirname)) == NULL ){
		perror("Errore con la cartella di destinazione");
		return -1;
	}
	
	if( optarg == NULL ){ // opzione -R senza parametri
		num_files = 0;
	}
	else{
		char *tmp;
		// parsing degli argomenti dell'opzione
		args_option = parseOptionArg(optarg);
		if( args_option == NULL ){
			perror("Errore durante l'elaborazione dei parametri");
			return -1;
		}
		
		// controllo che ci sia solo un parametro
		if( queueLen(args_option) != 1 ){
			fprintf(stderr, "Parametri non validi");
			return -1;
		}
		
		// controllo della validità del parametro numerico
		tmp = queueRemove(args_option);
		if( isIntNumber(tmp, &num_files) != 0 ){
			perror("Parametri non validi");
			return -1;
		}
		
		queueDestroy(args_option, free);
		free(tmp);
	}
	
	// lettura dei files da server
	if( (read_files = readNFiles(num_files, files_dirname)) == -1 ){
		perror("Errore durante la lettura dei files");
		return -1;
	}
	if( params->p_flag != 0 ) fprintf(stdout, "Sono stati scaricati con successo %d files dal server\n", read_files);
	
	if( dir != NULL ) free(dir);
	return 0;
}

/*
	Implementazione dell'opzione -t del client, salva in 
	params->delay_time il ritardo specificato in optarg.
	
	Restituisce 0 in caso di successo, -1 altrimenti
*/
int tOptionHandler(client_params_t *params, char *optarg){
	long delay_milliseconds, nanoseconds;
	time_t seconds;
	// verifica della validità del tempo
	if( isNumber(optarg, &delay_milliseconds) != 0 || delay_milliseconds < 0){
		fprintf(stderr, "Opzione -t non valida\n");
		return -1;
	}
	// trasformazione dei millisecondi in secondi e nanosecondi
	seconds = delay_milliseconds/1000;
	nanoseconds =  (delay_milliseconds - seconds*1000)*1000000;
	params->delay_time = (struct timespec) {seconds, nanoseconds};
	return 0;
}
