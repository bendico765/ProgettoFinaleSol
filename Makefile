CC = gcc
CFLAGS = -Wall -std=c99 $(INCLUDES) $(DEFINES)
INCLUDES = -I $(INC_FOLDER)
LIBS = -lpthread
DEFINES = -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE

INC_FOLDER = ./includes
SRC_FOLDER = ./src
OBJS_FOLDER = ./objs
BIN_FOLDER = ./bin

CLIENT_OBJS = client_utils.o utils.o client.o
SERVER_OBJS = signal_handler.o config_parser.o utils.o thread_utils.o queue.o server.o message.o storage.o icl_hash.o cache.o file.o
TEST_CLIENT_OBJS = utils.o testClient.o api.o message.o

all:  client server testClient

.PHONY: clean

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $(patsubst %.o,$(OBJS_FOLDER)/%.o,$^) -o $(BIN_FOLDER)/$@

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) $(patsubst %.o,$(OBJS_FOLDER)/%.o,$^) -o $(BIN_FOLDER)/$@ $(LIBS)
	
testClient: $(TEST_CLIENT_OBJS)
	$(CC) $(CFLAGS) $(patsubst %.o,$(OBJS_FOLDER)/%.o,$^) -o $(BIN_FOLDER)/$@
	
%.o: $(SRC_FOLDER)/%.c
	$(CC) $(CFLAGS) $^ -c -o $(OBJS_FOLDER)/$@

prova:
	echo $(patsubst %.o, $(OBJS_FOLDER)/%.c ,$(CLIENT_OBJS) ) 
		
clean:
	@echo "Rimozione files"
	-rm -f client server testClient
	-rm -f $(OBJS_FOLDER)/*.o
	-rm -f $(BIN_FOLDER)/*
	-rm -f ./expelledFilesDir/*
