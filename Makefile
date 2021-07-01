CC = gcc
CFLAGS = -Wall -std=c99 $(INCLUDES) $(DEFINES)
INCLUDES = -I $(INC_FOLDER)
LIBS = -lpthread
DEFINES = -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE

INC_FOLDER = includes
SRC_FOLDER = src
OBJS_FOLDER = objs
BIN_FOLDER = bin
SCRIPTS_FOLDER = scripts
CONFIG_FOLDER = config
TESTS_FOLDER = tests

CLIENT_OBJS = utils.o client.o queue.o api.o message.o client_handlers.o client_params.o
SERVER_OBJS = signal_handler.o config_parser.o utils.o thread_utils.o queue.o server.o message.o storage.o icl_hash.o cache.o file.o

all:  $(BIN_FOLDER)/client $(BIN_FOLDER)/server

.PHONY: clean test1 test2 serverStart

# generazione client
$(BIN_FOLDER)/client: $(patsubst %.o,$(OBJS_FOLDER)/%.o,$(CLIENT_OBJS))
	$(CC) $(CFLAGS) $^ -o $@

# generazione server
$(BIN_FOLDER)/server: $(patsubst %.o,$(OBJS_FOLDER)/%.o,$(SERVER_OBJS)) 
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
		
# generazione file oggetto
$(OBJS_FOLDER)/%.o: $(SRC_FOLDER)/%.c
	$(CC) $(CFLAGS) $^ -c -o $@
	
# PHONY TARGETS
		
clean:
	@echo "Rimozione files"
	-rm -f $(OBJS_FOLDER)/*.o
	-rm -f $(BIN_FOLDER)/*
	-rm -f ./expelledFilesDir/*
	-rm ./mysock
	
test1:
	@echo "Test1"
	make $(BIN_FOLDER)/client $(BIN_FOLDER)/server
	./$(SCRIPTS_FOLDER)/test1.sh $(BIN_FOLDER)/server $(CONFIG_FOLDER)/config1.txt $(TESTS_FOLDER)/test1.txt
	
test2:
	@echo "Test2"
	make $(BIN_FOLDER)/client $(BIN_FOLDER)/server
	./$(SCRIPTS_FOLDER)/test2.sh $(BIN_FOLDER)/server $(CONFIG_FOLDER)/config2.txt $(TESTS_FOLDER)/test2.txt
	
serverStart:
	valgrind -s --leak-check=full --track-origins=yes $(BIN_FOLDER)/server $(CONFIG_FOLDER)/config.txt
