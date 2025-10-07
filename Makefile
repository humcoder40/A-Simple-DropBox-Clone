CC = gcc
CFLAGS = -pthread -g -O0
SRC = $(wildcard src/*.c)
TARGET = server

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)
test_client: tests/single_client_test.c
	$(CC) -g -o tests/single_client_test tests/single_client_test.c

clean:
	rm -f $(TARGET) *.o
