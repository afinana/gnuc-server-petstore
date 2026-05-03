CC = cc
CFLAGS = -Wall -Wextra -g -O2 -I/usr/local/include/libmongoc-1.0 -I/usr/local/include/libbson-1.0
CFLAGS_DEBUG = -Wall -Wextra -g -O0 -DDEBUG -I/usr/local/include/libmongoc-1.0 -I/usr/local/include/libbson-1.0
LDFLAGS = -L/usr/local/lib -lmongoc-1.0 -lbson-1.0 -lmicrohttpd -lcjson
SRC = main.c handlers.c database.c
OBJ = $(SRC:.c=.o)
TARGET = petstore-api

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: clean $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)

run: all
	./$(TARGET)

.PHONY: all debug clean run
