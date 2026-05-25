CC       = cc
CFLAGS   = -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -g -O2 \
           -I/usr/local/include/libmongoc-1.0 \
           -I/usr/local/include/libbson-1.0
CFLAGS_DEBUG = -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -g -O0 -DDEBUG \
           -I/usr/local/include/libmongoc-1.0 \
           -I/usr/local/include/libbson-1.0
LDFLAGS  = -L/usr/local/lib -lmongoc-1.0 -lbson-1.0 -lmicrohttpd -lcjson

# Main application sources
SRC      = main.c handlers.c database.c
OBJ      = $(SRC:.c=.o)
TARGET   = petstore-api

# Shared library sources (no main()) — linked into test binary
SRCS_LIB = handlers.c
OBJS_LIB = $(SRCS_LIB:.c=.o)

# Test sources and objects
TEST_DIR     = tests
VENDOR_DIR   = $(TEST_DIR)/vendor/unity
STUBS_DIR    = $(TEST_DIR)/stubs
TEST_TARGET  = $(TEST_DIR)/test_runner

TEST_SRCS = $(TEST_DIR)/test_main.c \
            $(TEST_DIR)/test_handlers_pet.c \
            $(TEST_DIR)/test_handlers_user.c \
            $(STUBS_DIR)/database_stubs.c \
            $(VENDOR_DIR)/unity.c

TEST_CFLAGS = $(CFLAGS) \
              -I$(VENDOR_DIR) \
              -I$(STUBS_DIR) \
              -I. \
              -I$(TEST_DIR)

# Suppress Wpedantic / Wformat for vendored Unity code
UNITY_CFLAGS = -Wall -g -O2 \
               -I/usr/local/include/libmongoc-1.0 \
               -I/usr/local/include/libbson-1.0 \
               -I$(VENDOR_DIR) \
               -I$(STUBS_DIR) \
               -I.

# ============================================================
# Main targets
# ============================================================

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: clean $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
	rm -f $(TEST_TARGET) tests/*.o tests/stubs/*.o tests/vendor/unity/*.o

run: all
	./$(TARGET)

# ============================================================
# Test targets
# ============================================================

# Build test runner (each source compiled individually to control flags)
$(TEST_TARGET): $(TEST_SRCS) $(OBJS_LIB)
	$(CC) $(UNITY_CFLAGS) \
	    $(TEST_DIR)/test_main.c \
	    $(TEST_DIR)/test_handlers_pet.c \
	    $(TEST_DIR)/test_handlers_user.c \
	    $(STUBS_DIR)/database_stubs.c \
	    $(VENDOR_DIR)/unity.c \
	    $(SRCS_LIB) \
	    -o $(TEST_TARGET) $(LDFLAGS)

## Run all unit tests (exit code = number of failures)
test: $(TEST_TARGET)
	./$(TEST_TARGET)

## Run tests with verbose Unity output
test-verbose: $(TEST_TARGET)
	./$(TEST_TARGET) -v 2>&1 || true

## Run tests under Valgrind (requires valgrind to be installed)
valgrind: $(TEST_TARGET)
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_TARGET)

.PHONY: all debug clean run test test-verbose valgrind

