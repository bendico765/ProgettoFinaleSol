PARAMS = -Wall -std=c99 
INCLUDES = -I ../includes
LIBS = -lpthread

all:  client server

client: client_utils.c utils.c client.c
	gcc $(PARAMS) $(INCLUDES) $^ -o $@

server: config_parser.c utils.c thread_utils.c socket_utils.c queue.c server.c
	gcc $(PARAMS) $(INCLUDES) $^ -o $@ -lpthread
