#include "utils.h"
#include "api.h"
#include "message.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <string.h>

int main(){
	struct timespec abstime = {0,0};
	ce_less1(openConnection("./mysock", 999, abstime), "Fail openConnection");
	
	ce_less1(openFile("file2.txt", 1), "Errore openFile"); // file creato
	
	ce_less1(openFile("file3.txt", 1), "Errore openFile"); // file creato
		
	ce_less1(removeFile("file2.txt"), "Errore removeFile");
	
	ce_less1(closeConnection("./mysock"), "Fail closeConnection");
	
	return 0;
}
