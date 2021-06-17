#include "utils.h"
#include "socket_utils.h"
#include "api.h"
#include "message.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int main(){
	int socket_fd;
	struct timespec abstime = {2,1};
	message_header_t *tmp;
	
	ce_null(tmp = malloc(sizeof(message_header_t)), "Fail malloc message header");
	ce_less1(socket_fd = openConnection("./mysock", 999, abstime), "Fail openConnection");
	
	
	ce_less1(closeConnection("./mysock"), "Fail closeConnection");
	
	return 0;
}
