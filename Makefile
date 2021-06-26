CC = gcc
CFLAGS = -Wall -std=c99 $(INCLUDES) $(DEFINES)
INCLUDES = -I $(INC_FOLDER)
LIBS = -lpthread
DEFINES = -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE

INC_FOLDER = includes
SRC_FOLDER = src
OBJS_FOLDER = objs
BIN_FOLDER = bin

CLIENT_OBJS = utils.o client.o queue.o api.o message.o client_handlers.o client_params.o
SERVER_OBJS = signal_handler.o config_parser.o utils.o thread_utils.o queue.o server.o message.o storage.o icl_hash.o cache.o file.o

all:  $(BIN_FOLDER)/client $(BIN_FOLDER)/server

.PHONY: clean

# generazione client
$(BIN_FOLDER)/client: $(patsubst %.o,$(OBJS_FOLDER)/%.o,$(CLIENT_OBJS))
	$(CC) $(CFLAGS) $^ -o $@

# generazione server
$(BIN_FOLDER)/server: $(patsubst %.o,$(OBJS_FOLDER)/%.o,$(SERVER_OBJS)) 
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
		
# generazione file oggetto
$(OBJS_FOLDER)/%.o: $(SRC_FOLDER)/%.c
	$(CC) $(CFLAGS) $^ -c -o $@
		
clean:
	@echo "Rimozione files"
	-rm -f $(OBJS_FOLDER)/*.o
	-rm -f $(BIN_FOLDER)/*
	-rm -f ./expelledFilesDir/*
