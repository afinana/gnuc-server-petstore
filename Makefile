CC = cc
CFLAGS = -std=c11 -Wall -Wextra -g -O2 $(shell pkg-config --cflags libmongoc-1.0)
LDFLAGS = $(shell pkg-config --libs libmongoc-1.0) -lmicrohttpd
SRC = main.c handlers.c database.c
OBJ = $(SRC:.c=.o)
TARGET = petstore-api

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

debug: CFLAGS += -O0 -DDEBUG
debug: clean $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)

run: all
	./$(TARGET)

.PHONY: all debug clean run
