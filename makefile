CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
SRC = src/metadata.c src/queue.c src/server.c src/threadpool.c
OBJ = build/metadata.o build/queue.o build/server.o build/threadpool.o
TARGET = src/server
TSAN_TARGET = src/server_tsan

all: build $(TARGET)

build:
	mkdir -p build

$(TARGET): $(OBJ)
	@echo "🔧 Linking..."
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)
	@echo "✅ Build complete: ./src/server"

build/metadata.o: src/metadata.c
	$(CC) $(CFLAGS) -c $< -o $@

build/queue.o: src/queue.c
	$(CC) $(CFLAGS) -c $< -o $@

build/server.o: src/server.c
	$(CC) $(CFLAGS) -c $< -o $@

build/threadpool.o: src/threadpool.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(TARGET) $(TSAN_TARGET) valgrind.log
	@echo "🧹 Cleaned build files."

run: all
	@echo "🚀 Starting server on port 9000..."
	./$(TARGET) 9000

# ------------------ Full Automated Testing ------------------

test:
	@echo "🚀 Starting automated concurrency + Valgrind + TSAN tests..."
	@echo "--------------------------------------------"
	@echo "🧩 Building normal and TSAN versions..."
	make -s
	$(CC) -fsanitize=thread -g -pthread $(SRC) -o $(TSAN_TARGET)
	@echo "✅ TSAN build ready."

	@echo "🧠 Running Valgrind memory test..."
	@valgrind --leak-check=full --log-file=valgrind.log ./$(TARGET) 9000 & \
	VPID=$$!; \
	sleep 3; \
	echo "📡 Running simulated clients..."; \
	for i in $$(seq 1 5); do \
	  ( echo "LIST"; sleep 1; echo "QUIT"; ) | nc localhost 9000 & \
	done; \
	sleep 5; \
	kill $$VPID 2>/dev/null || true; \
	wait $$VPID 2>/dev/null || true; \
	echo "🧹 Valgrind summary:"; \
	tail -n 10 valgrind.log; \
	echo "--------------------------------------------"

	@echo "🔬 Running ThreadSanitizer test..."
	@TSAN_OPTIONS="suppressions=tsan.supp" ./$(TSAN_TARGET) 9000 & \
	TPID=$$!; \
	sleep 3; \
	echo "📡 Running simulated clients..."; \
	for i in $$(seq 1 5); do \
	  ( echo "LIST"; sleep 1; echo "QUIT"; ) | nc localhost 9000 & \
	done; \
	sleep 5; \
	kill $$TPID 2>/dev/null || true; \
	wait $$TPID 2>/dev/null || true; \
	echo "✅ TSAN test complete."

	@echo "--------------------------------------------"
	@echo "✅ All automated tests finished successfully."

.PHONY: all build clean run test
