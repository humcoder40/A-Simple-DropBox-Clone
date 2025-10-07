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

