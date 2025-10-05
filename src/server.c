#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "common.h"

#define PORT 9000
#define MAX_CLIENTS 10
#define NUM_CLIENT_THREADS 3

void *accept_loop(void *arg);
void *client_thread(void *arg);
void handle_client(int sock);

int main() {
    int server_fd;
    struct sockaddr_in addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen"); exit(1);
    }

    printf("[Server] Listening on port %d...\n", PORT);

    queue_init(&clientQueue, MAX_CLIENTS);
    queue_init(&taskQueue, 50);

    pthread_t acceptThread;
    pthread_create(&acceptThread, NULL, accept_loop, &server_fd);

    pthread_t clientThreads[NUM_CLIENT_THREADS];
    for (int i = 0; i < NUM_CLIENT_THREADS; i++)
        pthread_create(&clientThreads[i], NULL, client_thread, NULL);

    pthread_join(acceptThread, NULL);
    close(server_fd);
    return 0;
}

void *accept_loop(void *arg) {
    int server_fd = *(int*)arg;
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    while (1) {
        int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_sock < 0) {
            perror("accept failed");
            continue;
        }
        printf("[Server] Accepted client socket: %d\n", client_sock);
        queue_push(&clientQueue, (void*)(long)client_sock);
    }
    return NULL;
}

void *client_thread(void *arg) {
    while (1) {
        int client_sock = (int)(long)queue_pop(&clientQueue);
        handle_client(client_sock);
    }
    return NULL;
}

void handle_client(int sock) {
    char buf[256];
    send(sock, "Welcome to MiniDropBox! Enter command:\n", 39, 0);
    while (1) {
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        printf("[Client %d] %s\n", sock, buf);

        Task *t = malloc(sizeof(Task));
        t->client_sock = sock;
        sscanf(buf, "%127s %127s", t->username, t->filename);
        t->type = T_UPLOAD;  // just example
        queue_push(&taskQueue, t);
        printf("[Task] Pushed task for user %s, file %s\n", t->username, t->filename);
    }
    close(sock);
}
