CC = gcc
CFLAGS = -pthread -g -O0
SRC = $(wildcard src/*.c)

all: server
server: $(SRC)
	$(CC) $(CFLAGS) -o server $(SRC)

clean:
	rm -f server *.o
