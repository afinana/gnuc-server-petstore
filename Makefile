CC       = cc

# Dynamically resolve hiredis and libbson flags using pkg-config
HIREDIS_CFLAGS = $(shell pkg-config --cflags hiredis)
HIREDIS_LIBS   = $(shell pkg-config --libs hiredis)
BSON_CFLAGS    = $(shell pkg-config --cflags libbson-1.0)
BSON_LIBS      = $(shell pkg-config --libs libbson-1.0)

CFLAGS       = -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -g -O2 $(HIREDIS_CFLAGS) $(BSON_CFLAGS)
CFLAGS_DEBUG = -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -g -O0 -DDEBUG $(HIREDIS_CFLAGS) $(BSON_CFLAGS)
LDFLAGS      = $(HIREDIS_LIBS) $(BSON_LIBS) -lmicrohttpd -lcjson -lpthread

# Main application sources (database_utils.c is shared across main + tests)
SRC      = main.c handlers.c database.c database_utils.c
OBJ      = $(SRC:.c=.o)
TARGET   = petstore-api

# Library sources linked into the handler test binary (no main())
SRCS_LIB = handlers.c database_utils.c
OBJS_LIB = $(SRCS_LIB:.c=.o)

# ============================================================
# Test infrastructure — handler tests (test_runner)
# ============================================================

TEST_DIR    = tests
VENDOR_DIR  = $(TEST_DIR)/vendor/unity
STUBS_DIR   = $(TEST_DIR)/stubs
TEST_TARGET = $(TEST_DIR)/test_runner

TEST_SRCS = $(TEST_DIR)/test_main.c \
            $(TEST_DIR)/test_handlers_pet.c \
            $(TEST_DIR)/test_handlers_user.c \
            $(STUBS_DIR)/database_stubs.c \
            $(VENDOR_DIR)/unity.c

# Shared CFLAGS for test builds (relaxed: no -Wpedantic/-Wformat=2 for Unity)
TEST_CFLAGS = -Wall -g -O2 \
              $(HIREDIS_CFLAGS) \
              $(BSON_CFLAGS) \
              -I$(VENDOR_DIR) \
              -I$(STUBS_DIR) \
              -I.

# ============================================================
# Test infrastructure — database utils tests (test_runner_utils)
# ============================================================

TEST_UTILS_TARGET = $(TEST_DIR)/test_runner_utils
TEST_UTILS_SRCS   = $(TEST_DIR)/test_database_utils.c \
                    database_utils.c \
                    $(VENDOR_DIR)/unity.c

# No hiredis needed for utils tests — only libbson
TEST_UTILS_CFLAGS = -Wall -g -O2 $(BSON_CFLAGS) -I$(VENDOR_DIR) -I.
TEST_UTILS_LDFLAGS = $(BSON_LIBS)

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
	rm -f $(TEST_TARGET) $(TEST_UTILS_TARGET)
	rm -f tests/*.o tests/stubs/*.o tests/vendor/unity/*.o

run: all
	./$(TARGET)

# ============================================================
# Handler tests
# ============================================================

$(TEST_TARGET): $(TEST_SRCS) $(OBJS_LIB)
	$(CC) $(TEST_CFLAGS) \
	    $(TEST_DIR)/test_main.c \
	    $(TEST_DIR)/test_handlers_pet.c \
	    $(TEST_DIR)/test_handlers_user.c \
	    $(STUBS_DIR)/database_stubs.c \
	    $(VENDOR_DIR)/unity.c \
	    $(SRCS_LIB) \
	    -o $(TEST_TARGET) $(LDFLAGS)

## Run all handler unit tests
test: $(TEST_TARGET)
	./$(TEST_TARGET)

## Run handler tests with verbose Unity output
test-verbose: $(TEST_TARGET)
	./$(TEST_TARGET) -v 2>&1 || true

# ============================================================
# Database utils tests (no Redis required)
# ============================================================

$(TEST_UTILS_TARGET): $(TEST_UTILS_SRCS)
	$(CC) $(TEST_UTILS_CFLAGS) \
	    $(TEST_UTILS_SRCS) \
	    -o $(TEST_UTILS_TARGET) $(TEST_UTILS_LDFLAGS)

## Run database utils unit tests
test-utils: $(TEST_UTILS_TARGET)
	./$(TEST_UTILS_TARGET)

# ============================================================
# Run all test suites
# ============================================================

## Run every test suite (handler tests + database utils tests)
test-all: test test-utils

## Run all tests under Valgrind (requires valgrind to be installed)
valgrind: $(TEST_TARGET) $(TEST_UTILS_TARGET)
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_TARGET)
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_UTILS_TARGET)

.PHONY: all debug clean run test test-verbose test-utils test-all valgrind
