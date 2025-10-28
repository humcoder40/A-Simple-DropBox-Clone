# Compiler and flags
CC = gcc
CFLAGS = -pthread -g -O0 -Wall -Wextra
SRC = src/metadata.c src/queue.c src/server.c src/threadpool.c
OBJ = $(SRC:.c=.o)
TARGET = server
TSAN_TARGET = server_tsan

# Default build
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

# ThreadSanitizer build
$(TSAN_TARGET): $(SRC)
	$(CC) -fsanitize=thread -g -O0 -pthread -Wall -Wextra -o $(TSAN_TARGET) $(SRC)

# Run normal server
run: $(TARGET)
	./$(TARGET)

# Run under Valgrind for leak detection
valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET)

# Run with ThreadSanitizer to detect race conditions
tsan: $(TSAN_TARGET)
	./$(TSAN_TARGET)

# Automated concurrency test (spawns parallel telnet sessions)
test-concurrency:
	@echo "Launching 5 concurrent telnet clients for quick concurrency test..."
	@for i in 1 2 3 4 5; do \
		( sleep 1; echo "QUIT"; ) | telnet localhost 9000 & \
	done
	@sleep 3
	@echo "All test clients finished."

# Clean build artifacts
clean:
	rm -f $(TARGET) $(TSAN_TARGET) $(OBJ)
