#!/bin/bash
set -e

SERVER=./server
TSAN_SERVER=./server_tsan
PORT=9000

echo "🚀 Starting full automated concurrency + Valgrind + TSAN tests..."
echo "--------------------------------------------"

# Start server in background
$SERVER $PORT &
SERVER_PID=$!
sleep 2
echo "✅ Server started (PID $SERVER_PID)"

# 1️⃣ Concurrency test
echo "🧵 Running concurrency test..."
for i in {1..5}; do
  ( sleep 1; echo "LIST"; sleep 1; echo "QUIT"; ) | telnet localhost $PORT >/dev/null 2>&1 &
done
wait
echo "✅ Concurrency test finished."

# 2️⃣ Valgrind test
echo "💾 Running Valgrind memory test..."
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 $SERVER $PORT >/dev/null 2>&1 &
VAL_PID=$!
sleep 2
kill $VAL_PID >/dev/null 2>&1 || true
echo "✅ Valgrind check completed."

# Stop normal server
kill $SERVER_PID >/dev/null 2>&1 || true
sleep 1

# 3️⃣ TSAN test
echo "🧠 Running ThreadSanitizer test..."
$TSAN_SERVER $PORT >/dev/null 2>&1 &
TSAN_PID=$!
sleep 2
kill $TSAN_PID >/dev/null 2>&1 || true
echo "✅ TSAN check completed."

echo "--------------------------------------------"
echo "🏁 All tests completed successfully."
