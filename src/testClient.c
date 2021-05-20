#include "utils.h"
#include "socket_utils.h"
#include <stdio.h>
#include <unistd.h>

int main(){
	int socket_fd = initializeClientAndConnect("./mysock");
	char prova = 'c';
	fprintf(stderr, "Mando il messaggio1\n");
	writen(socket_fd, &prova, sizeof(char));
	readn(socket_fd, &prova, sizeof(char));
	fprintf(stderr, "Ho ricevuto come risposta1 %c\n", prova);
	prova = 'b';
	read(0, &prova, sizeof(char));
	fprintf(stderr, "Mando il messaggio2\n");
	writen(socket_fd, &prova, sizeof(char));
	readn(socket_fd, &prova, sizeof(char));
	fprintf(stderr, "Ho ricevuto come risposta2 %c\n", prova);
	read(0, &prova, sizeof(char));
	close(socket_fd);
}
