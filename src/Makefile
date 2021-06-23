CC = gcc
INCLUDES = -I ../includes
LIBS = -lpthread
DEFINES = -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE
CFLAGS = -Wall -std=c99 $(INCLUDES) $(DEFINES)

CLIENT_OBJS = client_utils.o utils.o client.o
SERVER_OBJS = signal_handler.o config_parser.o utils.o thread_utils.o socket_utils.o queue.o server.o message.o storage.o icl_hash.o cache.o file.o
TEST_CLIENT_OBJS = utils.o socket_utils.o testClient.o api.o message.o

all:  client server testClient

.PHONY: clean

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
	
testClient: $(TEST_CLIENT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@
		
clean:
	@echo "Rimozione files"
	-rm client server testClient
	-rm *.o
	-rm ../expelledFilesDir/*
