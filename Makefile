CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Iinclude
TARGET  = mypmap
TEST    = test_procfs

SRC     = src/main.c src/format.c src/options.c src/procfs.c
TEST_SRC = tests/test_procfs.c src/format.c src/options.c src/procfs.c

.PHONY: all test clean compile_commands

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $^ -o $@

test: $(TEST)
	./$(TEST)

$(TEST): $(TEST_SRC)
	$(CC) $(CFLAGS) $^ -o $@

compile_commands:
	bear -- make

clean:
	rm -f $(TARGET) $(TEST) compile_commands.json
