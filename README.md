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

