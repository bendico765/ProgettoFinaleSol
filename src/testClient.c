#include "utils.h"
#include "socket_utils.h"
#include "api.h"
#include "message.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int main(){
	struct timespec abstime = {2,1};
	ce_less1(openConnection("./mysock", 999, abstime), "Fail openConnection");
	
	ce_less1(openFile("file.txt", 1), "Errore openFile");
	
	ce_less1(closeConnection("./mysock"), "Fail closeConnection");
	
	return 0;
}
