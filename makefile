# ==============================
# Makefile for MiniDropBox Clone
# ==============================

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
SRC_DIR = src
OBJ_DIR = build
TARGET = server

# Collect all .c files automatically
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# ==============================
# Default target: normal build
# ==============================
all: build_dir $(TARGET)

$(TARGET): $(OBJS)
	@echo "🔧 Linking..."
	$(CC) $(CFLAGS) $(OBJS) -o $(SRC_DIR)/$(TARGET)
	@echo "✅ Build complete: ./src/$(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	@echo "🧩 Compiling $< ..."
	$(CC) $(CFLAGS) -c $< -o $@

build_dir:
	@mkdir -p $(OBJ_DIR)

# ==============================
# Run normally
# ==============================
run: all
	@echo "🚀 Starting server..."
	./src/$(TARGET) 9000

# ==============================
# Valgrind test
# ==============================
valgrind: all
	@echo "🧠 Running Valgrind (memory leak check)..."
	valgrind --leak-check=full --show-leak-kinds=all ./src/$(TARGET) 9000

# ==============================
# ThreadSanitizer build & run
# ==============================
tsan:
	@echo "🔬 Building ThreadSanitizer version..."
	$(CC) -fsanitize=thread -g -pthread $(SRCS) -o $(SRC_DIR)/server_tsan
	@echo "✅ TSAN build ready: ./src/server_tsan"
	@echo "Run it with: ./src/server_tsan 9000"

# ==============================
# Clean builds
# ==============================
clean:
	@echo "🧹 Cleaning build files..."
	rm -rf $(OBJ_DIR) ./src/$(TARGET) ./src/server_tsan
	@echo "✅ Clean complete!"

# ==============================
# Phony targets
# ==============================
.PHONY: all run valgrind tsan clean
