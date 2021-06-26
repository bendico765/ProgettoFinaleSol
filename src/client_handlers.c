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

/**/

queue_t* parseOptionArg(char *optarg){
	queue_t *queue;
	char *temp;
	char *dup;
	
	if( (queue = queueCreate()) == NULL ) return NULL;
	
	temp = strtok(optarg, ",");
	while( temp != NULL ){
		if( ( dup = strdup(temp) ) == NULL ){
			queueDestroy(queue, free);
			return NULL;
		}
		queueInsert(queue, (void*)dup);
		temp = strtok(NULL, ",");
	}
	return queue;
}

int getPathnamesFromDir(char *source_dir, int *num_files, queue_t *pathnames_queue){
	DIR *dir;
	struct dirent *file;
	
	dir = opendir(source_dir);
	if( dir == NULL ) return -1;
	
	errno = 0;
	while( (file = readdir(dir)) != NULL && *num_files != 0){
		// vado avanti se si tratta delle directory ./ e ../
		if(strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0){
			continue;
        }
        if( file->d_type != DT_DIR ){ // non è una directory
        	char* filename = strdup(file->d_name);
        	if( filename == NULL ) return -1;
        	if( queueInsert(pathnames_queue, (void*)filename) == NULL ){
        		return -1;
        	}
        	if( *num_files != -1 ) *num_files -= 1;
        }
        else{ // è una directory da visitare ricorsivamente
        	char *new_source = (char*) malloc(sizeof(char) * (strlen(source_dir) + strlen(file->d_name) + 2));
            strcat(new_source,source_dir);
            strcat(new_source,"/");
            strcat(new_source,file->d_name);
        	
        	if( getPathnamesFromDir(new_source, num_files, pathnames_queue) == -1 ) return -1;
        	free(new_source);
        }
	}
	if( errno != 0 ) return -1;
	
	free(dir);
	return 0;
}

int writeFilesToServer(char *socket_name, queue_t *files_list, char *expelled_files_dirname, struct timespec *delay_time, int print_flag){
	char *filename;
	struct stat statbuf;
	
	// provo ad aprire la connessione verso il server
	if( openConnection(socket_name, 0, (const struct timespec) {0,0}) == -1){
		perror("Impossibile connettersi al server");
		return -1;
	}
	
	// scrivo i files sul server
	while( (filename = (char*) queueRemove(files_list)) != NULL){
		// introduco il ritardo tra due richieste consecutive
		nanosleep(delay_time, NULL);
		// creo il nuovo file
		if( openFile(filename, 1) == -1 ){
			fprintf(stderr, "Impossibile creare il file %s: %s\n", filename, strerror(errno));
			free(filename);
			continue;
		}
		// file creato correttamente
		if( writeFile(filename, expelled_files_dirname) == -1 ){
			fprintf(stderr, "Errore durante la scrittura del file %s: %s\n", filename, strerror(errno));
			free(filename);
			continue;
		}
		if( print_flag != 0){
			if( stat(filename, &statbuf) == -1 ){
				fprintf(stdout, "Il file %s è stato scritto con successo\n", filename);
			}
			else{
				fprintf(stdout, "Il file %s [%ld bytes] [%.3f MB] è stato scritto con successo\n", filename, (size_t)statbuf.st_size, ((float)statbuf.st_size)/1000000);
			}
		}
		free(filename);
	}
	
	closeConnection(socket_name);
	return 0;
}

int readFileFromServer(char *socket_name, queue_t *files_list, char *dest_dirname, struct timespec *delay_time, int print_flag){
	char *filename;
	void *file_content;
	size_t file_size;
	
	// provo ad aprire la connessione verso il server
	if( openConnection(socket_name, 0, (const struct timespec) {0,0}) == -1){
		perror("Impossibile connettersi al server");
		return -1;
	}
	
	// leggo i files da server
	while( (filename = (char*) queueRemove(files_list)) != NULL){
		nanosleep(delay_time, NULL);
		if( readFile(filename, &file_content, &file_size) == -1 ){
			fprintf(stderr, "Errore durante la lettura del file %s: %s\n", filename, strerror(errno));
			free(filename);
			if( file_content != NULL ) free(file_content);
			continue;
		}
		if( print_flag != 0 ){
			fprintf(stdout, "Il file %s [%ld bytes] [%.3f MB] è stato scaricato con successo\n", filename, file_size, ((float)file_size)/1000000);
		}
		// scrivo (eventualmente) il file nella directory specificata
		if( dest_dirname != NULL ){
			FILE *file;
			char *pathname;
			char *new_path;
			size_t new_size;
			
			// estrapolo il nome del file scaricato 
			pathname = absolutePathToFilename(filename);
			if( pathname == NULL ){
				fprintf(stderr, "Errore durante la scrittura su disco del file %s: %s\n", filename, strerror(errno));
				free(filename);
				free(file_content);
				continue;
			}
			
			new_size = (strlen(dest_dirname) + strlen(pathname) + 2) * sizeof(char);
			new_path = (char*) malloc(new_size);
			
			memset(new_path, '\0', new_size);
			strcat(new_path, dest_dirname);
			strcat(new_path, "/");
			strcat(new_path, pathname);
			file = fopen(new_path, "wb");
			// scrivo il file su disco
			if( file == NULL || fwrite(file_content, file_size, 1, file) == -1){
				fprintf(stderr, "Errore durante la scrittura su disco del file %s: %s\n", filename, strerror(errno));
				free(new_path);
				free(file_content);
				free(filename);
				continue;
			}
			if( print_flag != 0 ) fprintf(stdout, "Il file %s è stato scritto in %s con successo\n", filename, dest_dirname);
			free(new_path);
			fclose(file);
		}
		free(file_content);
		free(filename);
	}
	
	closeConnection(socket_name);
	return 0;
}

/**/

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
	return 0;
}

int cOptionHandler(client_params_t *params, char *optarg){
	queue_t *args_option;
	char *filename;
	
	// faccio il parsing degli argomenti dell'opzione
	args_option = parseOptionArg(optarg);
	if( args_option == NULL ){
		perror("Errore durante l'elaborazione dei parametri");
		return -1;
	}
	
	// provo ad aprire la connessione verso il server
	if( openConnection(params->socket_name, 0, (const struct timespec) {0,0}) == -1){
		perror("Impossibile connettersi al server");
		return -1;
	}
	
	// rimuovo i files richiesti
	while( (filename = (char*) queueRemove(args_option)) != NULL){
		if( removeFile(filename) != 0 ){
			fprintf(stderr, "Errore durante la rimozione del file %s: %s\n", filename, strerror(errno));
			free(filename);
			continue;
		}
		if( params->p_flag != 0 ) fprintf(stdout, "File %s rimosso con successo\n", filename);
		free(filename);
	}
	
	closeConnection(params->socket_name);
	queueDestroy(args_option, free);
	return 0;
}

// params, parametri di -w, directory in cui scrivere i files espulsi
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
	fprintf(stderr, "Optarg: [%s]\n", optarg);
	args_option = parseOptionArg(optarg);
	if( args_option == NULL ){
		perror("Errore durante l'elaborazione dei parametri");
		return -1;
	}
	
	num_args = queueLen(args_option);
	switch(num_args){
		case 1: // numero files non specificato
			source_directory = (char*) queueRemove(args_option);
			num_files = -1;
			break;
		case 2: 
			source_directory = (char*) queueRemove(args_option);
			tmp = (char*) queueRemove(args_option);
			if( isIntNumber(tmp, &num_files) != 0 ){
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
	
	// salvo in to_send_files i pathanames dei files
	// da mandare al server
	if( getPathnamesFromDir(source_directory, &num_files, to_send_files) != 0 ){
		perror("Errore durante la lettura dei files da mandare");
		return -1;
	}
	
	// mando i files al server
	if( writeFilesToServer(params->socket_name, args_option, expelled_files_dirname, &params->delay_time, params->p_flag) == -1 ) return -1;
	
	if( dir != NULL ) free(dir);
	free(source_directory);
	queueDestroy(args_option, free);
	queueDestroy(to_send_files, free);
	return 0;
}

int WOptionHandler(client_params_t *params, char *optarg, char *expelled_files_dirname){
	queue_t *args_option;
	DIR *dir;
	
	// verifica l'esistenza della eventuale directory
	dir = NULL;
	if( expelled_files_dirname != NULL && (dir = opendir(expelled_files_dirname)) == NULL ){
		perror("Errore con la cartella di destinazione");
		return -1;
	}
	
	// faccio il parsing degli argomenti dell'opzione
	args_option = parseOptionArg(optarg);
	if( args_option == NULL ){
		perror("Errore durante l'elaborazione dei parametri");
		return -1;
	}

	// scrivo i files sul server
	if( writeFilesToServer(params->socket_name, args_option, expelled_files_dirname, &params->delay_time, params->p_flag) == -1 ) return -1;
	
	if( dir != NULL ) free(dir);
	queueDestroy(args_option, free);
	return 0;
}

int rOptionHandler(client_params_t *params, char *optarg, char *files_dirname){
	queue_t *args_option;
	DIR *dir;
	
	// verifica l'esistenza della eventuale directory
	dir = NULL;
	if( files_dirname != NULL && (dir = opendir(files_dirname)) == NULL ){
		perror("Errore con la cartella di destinazione");
		return -1;
	}
	
	// faccio il parsing degli argomenti dell'opzione
	args_option = parseOptionArg(optarg);
	if( args_option == NULL ){
		perror("Errore durante l'elaborazione dei parametri");
		return -1;
	}
	
	// leggo il contenuto dei files ed (se specificato) li scrivo
	// nella directory
	if( readFileFromServer(params->socket_name, args_option, files_dirname, &params->delay_time, params->p_flag) == -1){
		perror("Errore durante la lettura dei files");
	}
	
	if( dir != NULL ) free(dir);
	queueDestroy(args_option, free);
	return 0;
}

int ROptionHandler(client_params_t *params, char *optarg, char *files_dirname){
	queue_t *args_option;
	DIR *dir;
	int num_files;
	int read_files;
	
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
		// faccio il parsing degli argomenti dell'opzione
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
		
		// controllo la validità del parametro numerico
		tmp = queueRemove(args_option);
		if( isIntNumber(tmp, &num_files) != 0 ){
			perror("Parametri non validi");
			return -1;
		}
		
		free(tmp);
	}
	
	// provo ad aprire la connessione verso il server
	if( openConnection(params->socket_name, 0, (const struct timespec) {0,0}) == -1){
		perror("Impossibile connettersi al server");
		return -1;
	}
	
	// leggo i files
	if( (read_files = readNFiles(num_files, files_dirname)) == -1 ){
		perror("Errore durante la lettura dei files");
		return -1;
	}
	if( params->p_flag != 0 ) fprintf(stdout, "Sono stati scaricati con successo %d files dal server\n", read_files);
	
	closeConnection(params->socket_name);
	if( dir != NULL ) free(dir);
	queueDestroy(args_option, free);
	return 0;
}

int tOptionHandler(client_params_t *params, char *optarg){
	long delay_milliseconds, nanoseconds;
	time_t seconds;
	// verifico la validità del tempo
	if( isNumber(optarg, &delay_milliseconds) != 0 || delay_milliseconds < 0){
		fprintf(stderr, "Opzione -p non valida\n");
		return -1;
	}
	// trasformo i millisecondi in secondi e nanosecondi
	seconds = delay_milliseconds/1000;
	nanoseconds =  (delay_milliseconds - seconds*1000)*1000000;
	params->delay_time = (struct timespec) {seconds, nanoseconds};
	return 0;
}
