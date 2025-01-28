CC = cc
#CFLAGS = -Wall  -Wextra -g -O0 -DDEBUG -I/usr/include/libmongoc-1.0 -I/usr/include/libbson-1.0
CFLAGS = -Wall  -Wextra -g -O0 -DDEBUG -I/usr/include/libmongoc-1.0 -I/usr/include/libbson-1.0
LDFLAGS = -lmongoc-1.0 -lbson-1.0 -lmicrohttpd
SRC = main.c handlers.c database.c 
OBJ = $(SRC:.c=.o)
TARGET = petstore-api

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(OBJ) $(TARGET)

run: all
	./$(TARGET)

.PHONY: all clean run


