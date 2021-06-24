#include "utils.h"
#include "api.h"
#include "message.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include <string.h>

int main(){
	struct timespec abstime = {0,0};
	void *buf = strdup("ciao!");
	size_t size = strlen(buf)*sizeof(char);
	ce_less1(openConnection("./mysock", 999, abstime), "Fail openConnection");
		
	ce_less1(openFile("./files/file2", 1), "Errore openFile"); // file creato
	ce_less1(writeFile("./files/file2", (const char*)"./expelledFilesDir"), "Errore writeFile"); // file scritto
	
	ce_less1(openFile("./files/file1", 1), "Errore openFile"); // file creato
	ce_less1(writeFile("./files/file1", (const char*)"./expelledFilesDir"), "Errore writeFile"); // file scritto
	
	ce_less1(appendToFile("./files/file2", buf, size, NULL), "Errore appendToFile");
	
	ce_less1(readNFiles(3, (const char*)"./expelledFilesDir"), "Errore readNFiles");
	
	ce_less1(closeFile("./files/file2"), "Errore closeFile"); // file chiuso
	
	ce_less1(closeFile("./files/file1"), "Errore closeFile"); // file chiuso
	
	ce_less1(removeFile("./files/file2"), "Errore removeFile"); // file rimosso
	
	ce_less1(removeFile("./files/file1"), "Errore removeFile"); // file rimosso
	
	ce_less1(closeConnection("./mysock"), "Fail closeConnection");
	
	free(buf);
	return 0;
}
