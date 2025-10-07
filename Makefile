# Compiler and flags
CC = gcc
CFLAGS = -pthread -g -O0 -Wall -Wextra
SRC = src/metadata.c src/queue.c src/server.c src/threadpool.c
OBJ = $(SRC:.c=.o)
TARGET = server

# Default build
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

# Run the server
run: $(TARGET)
	./$(TARGET)

# Run server under Valgrind for leak detection
valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

# Clean up build artifacts
clean:
	rm -f $(TARGET) $(OBJ)
