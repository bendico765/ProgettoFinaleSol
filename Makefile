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

SERVER_FOLDER = server
CLIENT_FOLDER = client
UTILS_FOLDER = utils

SERVER_OBJS = server.o signal_handler.o config_parser.o thread_utils.o storage.o icl_hash.o fifo_cache.o lru_cache.o file.o
CLIENT_OBJS = client.o api.o client_handlers.o client_params.o
UTILS_OBJS = utils.o message.o queue.o

all:  $(BIN_FOLDER)/client $(BIN_FOLDER)/server

.PHONY: clean test1 test2 LRU_test2 serverStart

# generazione client
$(BIN_FOLDER)/client: $(patsubst %.o,$(OBJS_FOLDER)/$(CLIENT_FOLDER)/%.o,$(CLIENT_OBJS)) $(patsubst %.o,$(OBJS_FOLDER)/$(UTILS_FOLDER)/%.o,$(UTILS_OBJS))
	$(CC) $(CFLAGS) $^ -o $@
	
$(OBJS_FOLDER)/$(CLIENT_FOLDER)/%.o: $(SRC_FOLDER)/$(CLIENT_FOLDER)/%.c 
	$(CC) $(CFLAGS) $^ -c -o $@

# generazione server
$(BIN_FOLDER)/server: $(patsubst %.o,$(OBJS_FOLDER)/$(SERVER_FOLDER)/%.o,$(SERVER_OBJS)) $(patsubst %.o,$(OBJS_FOLDER)/$(UTILS_FOLDER)/%.o,$(UTILS_OBJS))
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)
	
$(OBJS_FOLDER)/$(SERVER_FOLDER)/%.o: $(SRC_FOLDER)/$(SERVER_FOLDER)/%.c
	$(CC) $(CFLAGS) $^ -c -o $@
	
# generazione file oggetto utils
$(OBJS_FOLDER)/$(UTILS_FOLDER)/%.o: $(SRC_FOLDER)/$(UTILS_FOLDER)/%.c
	$(CC) $(CFLAGS) $^ -c -o $@
		
# PHONY TARGETS
		
clean:
	@echo "Rimozione files"
	-rm -f $(OBJS_FOLDER)/$(SERVER_FOLDER)/*.o
	-rm -f $(OBJS_FOLDER)/$(CLIENT_FOLDER)/*.o
	-rm -f $(OBJS_FOLDER)/$(UTILS_FOLDER)/*.o
	-rm -f $(BIN_FOLDER)/*
	-rm -f ./expelledFilesDir/*
	-rm -f ./log.txt
	-rm ./mysock
	
test1:
	@echo "FIFO Test1"
	make
	./$(SCRIPTS_FOLDER)/test1.sh $(BIN_FOLDER)/server $(CONFIG_FOLDER)/FIFO_config1.txt $(TESTS_FOLDER)/FIFO_test1.txt
	
test2:
	@echo "FIFO Test2"
	make
	./$(SCRIPTS_FOLDER)/test2.sh $(BIN_FOLDER)/server $(CONFIG_FOLDER)/FIFO_config2.txt $(TESTS_FOLDER)/FIFO_test2.txt
	
LRU_test2:
	@echo "LRU Test2"
	make
	./$(SCRIPTS_FOLDER)/test2.sh $(BIN_FOLDER)/server $(CONFIG_FOLDER)/LRU_config2.txt $(TESTS_FOLDER)/LRU_test2.txt
	
serverStart:
	make
	valgrind -s --leak-check=full --track-origins=yes $(BIN_FOLDER)/server $(CONFIG_FOLDER)/config.txt
