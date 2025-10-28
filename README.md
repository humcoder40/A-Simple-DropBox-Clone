# A Simple DropBox Clone – Phase 1

## Build
```bash
make clean
make
# A-Simple-DropBox-Clone
./server
telnet 127.0.0.1 9000
signup name password
login name password
upload name filename
delete name filename
list name
download name filename
quit
to check memory leak before running "./server"
valgrind --leak-check=full --show-leak-kinds=all ./server

to compile the test script file
chmod +x full_test.sh

no as we have done valgrind and tsan testing automatic so we combined all concurrency,valgrind and tsan
u do that by simply writing 
make clean
make
make test

to do manual command run u do
make clean
make
make run

for manual valgrind
# 1️⃣ build normally first
make
# 2️⃣ run server under valgrind
valgrind --leak-check=full --show-leak-kinds=all ./src/server 9000

for manual tsan
# 1️⃣ build tsan version
gcc -fsanitize=thread -g -pthread src/metadata.c src/queue.c src/server.c src/threadpool.c -o src/server_tsan
# 2️⃣ run it
TSAN_OPTIONS="suppressions=tsan.supp" ./src/server_tsan 9000

