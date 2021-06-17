PARAMS = -Wall -std=c99 
INCLUDES = -I ../includes
LIBS = -lpthread

all:  client server testClient

.PHONY: clean test_stressConnection

client: client_utils.c utils.c client.c
	gcc $(PARAMS) $(INCLUDES) $^ -o $@

server: signal_handler.c config_parser.c utils.c thread_utils.c socket_utils.c queue.c server.c message.c
	gcc $(PARAMS) $(INCLUDES) $^ -o $@ -lpthread
	
testClient: utils.c socket_utils.c testClient.c api.c message.c
	gcc $(PARAMS) $(INCLUDES) $^ -o $@
	
clean:
	@echo "Rimozione files"
	-rm client server testClient
