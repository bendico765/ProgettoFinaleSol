CC = gcc
CFLAGS = -Wall -std=c99 $(DEFINES)
DEFINES = -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE

INC_FOLDER = includes
SRC_FOLDER = src
OBJS_FOLDER = objs
BIN_FOLDER = bin
LIBS_FOLDER = libs
SCRIPTS_FOLDER = scripts
CONFIG_FOLDER = config
TESTS_FOLDER = tests

SERVER_FOLDER = server
CLIENT_FOLDER = client
UTILS_FOLDER = utils
STORAGE_FOLDER = storage
API_FOLDER = api

all:  $(BIN_FOLDER)/client $(BIN_FOLDER)/server

.PHONY: clean test1 test2 LRU_test2 serverStart

# UTILS
UTILS_OBJS = utils.o message.o queue.o
UTILS_INCLUDES = -I $(INC_FOLDER)/$(UTILS_FOLDER)
UTILS_DEPENDENCIES = $(SRC_FOLDER)/$(UTILS_FOLDER)/%.c

$(OBJS_FOLDER)/$(UTILS_FOLDER)/%.o: $(UTILS_DEPENDENCIES)
	$(CC) $(CFLAGS) $(UTILS_INCLUDES) $^ -c -o $@

# CLIENT
CLIENT_OBJS = client.o client_handlers.o client_params.o
CLIENT_INCLUDES = -I $(INC_FOLDER)/$(CLIENT_FOLDER) -I $(INC_FOLDER)/$(API_FOLDER) -I $(INC_FOLDER)/$(UTILS_FOLDER)
CLIENT_DEPENDECIES = $(patsubst %.o,$(OBJS_FOLDER)/$(CLIENT_FOLDER)/%.o,$(CLIENT_OBJS)) $(patsubst %.o,$(OBJS_FOLDER)/$(UTILS_FOLDER)/%.o,$(UTILS_OBJS)) $(LIBS_FOLDER)/libapi.so
CLIENT_LIBS = -lapi

$(BIN_FOLDER)/client: $(CLIENT_DEPENDECIES)
	$(CC) $(CFLAGS) -L $(LIBS_FOLDER) -Wl,-rpath,$(LIBS_FOLDER) $^ -o $@ $(CLIENT_LIBS)
	
$(OBJS_FOLDER)/$(CLIENT_FOLDER)/%.o: $(SRC_FOLDER)/$(CLIENT_FOLDER)/%.c 
	$(CC) $(CFLAGS) $(CLIENT_INCLUDES) $^ -c -o $@

# SERVER
SERVER_OBJS = server.o signal_handler.o config_parser.o thread_utils.o file.o
SERVER_INCLUDES = -I $(INC_FOLDER)/$(SERVER_FOLDER) -I $(INC_FOLDER)/$(STORAGE_FOLDER) -I $(INC_FOLDER)/$(UTILS_FOLDER)
SERVER_DEPENDENCIES = $(patsubst %.o,$(OBJS_FOLDER)/$(SERVER_FOLDER)/%.o,$(SERVER_OBJS)) $(patsubst %.o,$(OBJS_FOLDER)/$(UTILS_FOLDER)/%.o,$(UTILS_OBJS)) $(LIBS_FOLDER)/libstorage.so
SERVER_LIBS = -lstorage -lpthread

$(BIN_FOLDER)/server: $(SERVER_DEPENDENCIES)
	$(CC) $(CFLAGS) -L $(LIBS_FOLDER) -Wl,-rpath,$(LIBS_FOLDER) $^ -o $@ $(SERVER_LIBS)
	
$(OBJS_FOLDER)/$(SERVER_FOLDER)/%.o: $(SRC_FOLDER)/$(SERVER_FOLDER)/%.c
	$(CC) $(CFLAGS) $(SERVER_INCLUDES) $^ -c -o $@

# LIBRERIA STORAGE
STORAGE_OBJS = storage.o icl_hash.o fifo_cache.o lru_cache.o
STORAGE_INCLUDES = -I $(INC_FOLDER)/$(STORAGE_FOLDER) -I $(INC_FOLDER)/$(UTILS_FOLDER)
STORAGE_DEPENDENCIES = $(patsubst %.o,$(OBJS_FOLDER)/$(STORAGE_FOLDER)/%.o,$(STORAGE_OBJS))

$(LIBS_FOLDER)/libstorage.so: $(STORAGE_DEPENDENCIES)
	$(CC) -shared -o $@ $^
	
$(OBJS_FOLDER)/$(STORAGE_FOLDER)/%.o: $(SRC_FOLDER)/$(STORAGE_FOLDER)/%.c
	$(CC) $(CFLAGS) $(STORAGE_INCLUDES) $^ -c -fPIC -o $@
	
# LIBRERIA API
API_OBJS = api.o
API_INCLUDES = -I $(INC_FOLDER)/$(API_FOLDER) -I $(INC_FOLDER)/$(UTILS_FOLDER)
API_DEPENDENCIES = $(patsubst %.o,$(OBJS_FOLDER)/$(API_FOLDER)/%.o,$(API_OBJS))

$(LIBS_FOLDER)/libapi.so: $(API_DEPENDENCIES)
	$(CC) -shared -o $@ $^
	
$(OBJS_FOLDER)/$(API_FOLDER)/%.o: $(SRC_FOLDER)/$(API_FOLDER)/%.c
	$(CC) $(CFLAGS) $(API_INCLUDES) $^ -c -fPIC -o $@

			
# PHONY TARGETS
		
clean:
	@echo "Rimozione files"
	-rm -f $(OBJS_FOLDER)/$(SERVER_FOLDER)/*.o
	-rm -f $(OBJS_FOLDER)/$(CLIENT_FOLDER)/*.o
	-rm -f $(OBJS_FOLDER)/$(STORAGE_FOLDER)/*.o
	-rm -f $(OBJS_FOLDER)/$(API_FOLDER)/*.o
	-rm -f $(OBJS_FOLDER)/$(UTILS_FOLDER)/*.o
	-rm -f $(LIBS_FOLDER)/*
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
