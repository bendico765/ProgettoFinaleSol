PARAMS = -Wall -std=c99 
INCLUDES = -I ../includes
LIBS = -lpthread
DEFINES = -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE

all:  client server testClient

.PHONY: clean

client: client_utils.c utils.c client.c
	gcc $(PARAMS) $(INCLUDES) $(DEFINES) $^ -o $@

server: signal_handler.c config_parser.c utils.c thread_utils.c socket_utils.c queue.c server.c message.c storage.c icl_hash.c cache.c file.c
	gcc $(PARAMS) $(INCLUDES) $(DEFINES) $^ -o $@ -lpthread
	
testClient: utils.c socket_utils.c testClient.c api.c message.c
	gcc $(PARAMS) $(INCLUDES) $(DEFINES) $^ -o $@
	
clean:
	@echo "Rimozione files"
	-rm client server testClient
