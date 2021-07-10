#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "api.h"
#include "client_params.h"
#include "client_handlers.h"

int main(int argc, char *argv[]){
	client_params_t *params;
	char *optstring;
	int opt, termination_flag;
	
	optstring = ":hf:w:W:D:r:R:d:t:c:p";
	termination_flag = 0;
	ce_null( params = paramsCreate(), "Errore con l'inizializzazione dei parametri client" )

	// parsing degli argomenti
	while( termination_flag == 0 && (opt = getopt(argc, argv, optstring)) != -1 ){
		switch(opt){
			case 'h':
				hOptionHandler(argv[0]);
				termination_flag = 1;
				break;
			case 'f': // specifica del nome del socket AF_UNIX
				if( fOptionHandler(params, optarg) != 0 ) termination_flag = 1;
				break;
			case 'w':
				if( optind < argc && strcmp("-D", argv[optind]) == 0 ){ // controllo se il prossimo parametro è -D
					// salvo la richiesta per il parametro -D
					if( (params->w_args = strdup(optarg)) == NULL ){
						perror("Errore durante l'esecuzione del client");
						termination_flag = 1;
					}
				}
				else{ // effettuo immediatamente la richiesta
					if( wOptionHandler(params, optarg, NULL) == -1) termination_flag = 1;
				}
				break;
			case 'W':
				if( optind < argc && strcmp("-D", argv[optind]) == 0 ){ // controllo se il prossimo parametro è -D
					// salvo la richiesta per il parametro -D
					if( (params->W_args = strdup(optarg)) == NULL ){
						perror("Errore durante l'esecuzione del client");
						termination_flag = 1;
					}
				}
				else{ // effettuo immediatamente la richiesta
					if( WOptionHandler(params, optarg, NULL) == -1) termination_flag = 1;
				}
				break;
			case 'D':
				if( params->w_args == NULL && params->W_args == NULL){ // errore
					fprintf(stderr, "Opzione -D non valida, non è preceduta da -w o -W\n");
					termination_flag = 1;
				}
				else{
					// eseguo le richieste pendenti specificando la directory
					if( params->w_args != NULL ){
						if( wOptionHandler(params, params->w_args, optarg) == -1 ) termination_flag = 1;
						free(params->w_args);
						params->w_args = NULL;
					}
					if( params->W_args != NULL ){
						if( WOptionHandler(params, params->W_args, optarg) == -1 ) termination_flag = 1;
						free(params->W_args);
						params->W_args = NULL;
					}
				}
				break;
			case 'r':
				if( optind < argc && strcmp("-d", argv[optind]) == 0 ){ // controllo se il prossimo parametro è -d
					// salvo la richiesta per il parametro -d
					params->r_args = strdup(optarg);
					if( params->r_args == NULL ){
						perror("Errore durante l'esecuzione del client");
						termination_flag = 1;
					}
				}
				else{ // effettuo immediatamente la richiesta
					if( rOptionHandler(params, optarg, NULL) == -1) termination_flag = 1;
				}
				break;
			case 'R':
				if( strcmp(optarg, "-d") == 0 ){ // non è stato specificato il parametro di -R
					optind -= 1; // riporto indietro l'indice di getopt
					optarg = "0";
				}
				if( optind < argc && strcmp("-d", argv[optind]) == 0 ){ // controllo se il prossimo parametro è -d
					// salvo la richiesta per il parametro -d
					if( (params->R_args = strdup(optarg)) == NULL ){
						perror("Errore durante l'esecuzione del client");
						termination_flag = 1;
					}
				}
				else{ // effettuo immediatamente la richiesta
					if( ROptionHandler(params, optarg, NULL) == -1) termination_flag = 1;
				}
				break;
			case 'd':
				if( params->r_args == NULL && params->R_args == NULL){ // errore
					fprintf(stderr, "Opzione -d non valida, non è preceduta da -r o -R\n");
					termination_flag = 1;
				}
				else{
					// eseguo le richieste pendenti specificando la directory
					if( params->r_args != NULL ){
						if( rOptionHandler(params, params->r_args, optarg) == -1 ) termination_flag = 1;
						free(params->r_args);
						params->r_args = NULL;
					}
					if( params->R_args != NULL ){
						if( ROptionHandler(params, params->R_args, optarg) == -1 ) termination_flag = 1;
						free(params->R_args);
						params->R_args = NULL;
					}
				}
				break;
			case 't': // tempo in millisecondi che intercorre tra richieste successivo
				if( tOptionHandler(params, optarg) != 0 ) termination_flag = 1;
				break;
			case 'c':
				if( cOptionHandler(params, optarg) != 0 ) termination_flag = 1;
				break;
			case 'p': // abilitazione delle stampe su stdout
				if( params->p_flag == 0 ){
					params->p_flag = 1;
				}
				else{
					fprintf(stderr, "Opzione -p già specificata\n");
					termination_flag = 1;
				}
				break;
			case '?':
				fprintf(stdout, "Opzione sconosciuta %c\n", optopt);
				termination_flag = 1;
				break;
			case ':': // argomento mancante
				if( optopt == 'R' ){ // R ha un parametro opzionale
					if( optind < argc && strcmp("-d", argv[optind]) == 0 ){ // controllo se il prossimo parametro è -d
						// salvo la richiesta per il parametro 0
						params->R_args = strdup("0");
						if( params->R_args == NULL ){
							perror("Errore durante l'esecuzione del client");
							termination_flag = 1;
						}
					}
					else{ // effettuo immediatamente la richiesta
						if( ROptionHandler(params, NULL, NULL) == -1) termination_flag = 1;
					}
				}
				else{
					fprintf(stderr, "Opzione -%c manca di argomento\n", opt);
					termination_flag = 1;
				}
				break;
		}
	}
	if( params->socket_name[0] != '\0' && closeConnection(params->socket_name) == -1){
		fprintf(stderr, "Errore durante la chiusura della connessione con il server\n");
	}
	paramsDestroy(params);
	return 0;
}

