CC = cc
CFLAGS_PRO = -Wall  -Wextra -g -O2 -I/usr/include/hiredis -I/usr/include/cjson
CFLAGS = -Wall  -Wextra -g -O0 -DDEBUG -I/usr/include/hiredis -I/usr/include/cjson
LDFLAGS = -lhiredis -lcjson -lmicrohttpd
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
