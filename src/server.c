#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>
#include "common.h"

// forward from threadpool.c
void start_worker_pool();

#define PORT 9000
#define MAX_CLIENTS 10
#define NUM_CLIENT_THREADS 3

extern Queue clientQueue;
extern Queue taskQueue;

void *accept_loop(void *arg);
void *client_thread(void *arg);
void handle_client(int sock);

int main(int argc, char **argv) {
    int server_fd;
    struct sockaddr_in addr;

    // ensure storage root exists
    mkdir(STORAGE_ROOT, 0755);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
    if (listen(server_fd, MAX_CLIENTS) < 0) { perror("listen"); exit(1); }

    printf("[Server] Listening on port %d...\n", PORT);

    queue_init(&clientQueue, MAX_CLIENTS);
    queue_init(&taskQueue, 100);

    start_worker_pool();

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

// helper to read n bytes exactly
static ssize_t readn(int fd, void *buf, size_t n) {
    size_t total = 0;
    char *p = buf;
    while (total < n) {
        ssize_t r = recv(fd, p + total, n - total, 0);
        if (r <= 0) return r;
        total += r;
    }
    return total;
}

// client thread loop
void *client_thread(void *arg) {
    (void)arg;
    while (1) {
        int client_sock = (int)(long)queue_pop(&clientQueue);
        handle_client(client_sock);
    }
    return NULL;
}

void send_line(int sock, const char *s) {
    send(sock, s, strlen(s), 0);
}

void handle_client(int sock) {
    char buf[4096];
    send_line(sock, "Welcome to MiniDropBox!\nCommands:\nSIGNUP user pass\nLOGIN user pass\nUPLOAD <username> <filename>\\n<filesize>\\n<bytes>\nDOWNLOAD <username> <filename>\nDELETE <username> <filename>\nLIST <username>\n");
    while (1) {
        int n = recv(sock, buf, sizeof(buf)-1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        // trim newline
        char *p = buf;
        while (*p && (*p=='\r' || *p=='\n')) p++;
        // parse command
        char cmd[32], username[64], filename[256];
        int ret = sscanf(buf, "%31s %63s %255s", cmd, username, filename);
        if (ret < 1) continue;

        if (strcasecmp(cmd, "UPLOAD") == 0) {
            if (ret < 3) { send_line(sock, "ERROR: UPLOAD requires username and filename\n"); continue; }
            // next: read a line containing filesize
            send_line(sock, "READY_FOR_SIZE\n");
            int m = recv(sock, buf, sizeof(buf)-1, 0);
            if (m <= 0) break;
            buf[m] = '\0';
            long filesize = atol(buf);
            if (filesize <= 0) { send_line(sock, "ERROR: invalid size\n"); continue; }

            // prepare tmp path
            char tmppath[PATH_MAX_LEN];
            snprintf(tmppath, sizeof(tmppath), "%s/%s.upload.%d.tmp", STORAGE_ROOT, username, (int)getpid());
            // ensure user temp dir exists
            char userdir[PATH_MAX_LEN];
            snprintf(userdir, sizeof(userdir), "%s/%s", STORAGE_ROOT, username);
            mkdir(STORAGE_ROOT, 0755);
            mkdir(userdir, 0755);

            FILE *f = fopen(tmppath, "wb");
            if (!f) {
                send_line(sock, "ERROR: cannot create tmp\n");
                continue;
            }
            // read exactly filesize bytes
            long remaining = filesize;
            char buf2[8192];
            while (remaining > 0) {
                ssize_t toread = remaining > (long)sizeof(buf2) ? sizeof(buf2) : remaining;
                ssize_t r = readn(sock, buf2, toread);
                if (r <= 0) { fclose(f); unlink(tmppath); goto client_end; }
                fwrite(buf2, 1, r, f);
                remaining -= r;
            }
            fclose(f);

            // create task and wait for worker
            Task *t = malloc(sizeof(Task));
            memset(t,0,sizeof(Task));
            t->type = T_UPLOAD;
            strncpy(t->username, username, sizeof(t->username)-1);
            strncpy(t->filename, filename, sizeof(t->filename)-1);
            t->client_sock = sock;
            strncpy(t->tmp_path, tmppath, sizeof(t->tmp_path)-1);
            pthread_mutex_init(&t->resp_mtx, NULL);
            pthread_cond_init(&t->resp_cond, NULL);
            t->done = 0;

            queue_push(&taskQueue, t);

            // wait for worker
            pthread_mutex_lock(&t->resp_mtx);
            while (!t->done) pthread_cond_wait(&t->resp_cond, &t->resp_mtx);
            pthread_mutex_unlock(&t->resp_mtx);

            if (t->status == 0) send_line(sock, "UPLOAD OK\n");
            else {
                char tmp[512];
                snprintf(tmp, sizeof(tmp), "UPLOAD ERROR: %s\n", t->errmsg);
                send_line(sock, tmp);
            }
            // cleanup
            pthread_mutex_destroy(&t->resp_mtx);
            pthread_cond_destroy(&t->resp_cond);
            free(t);
        }
        else if (strcasecmp(cmd, "DOWNLOAD") == 0) {
            if (ret < 3) { send_line(sock, "ERROR: DOWNLOAD requires username and filename\n"); continue; }
            Task *t = malloc(sizeof(Task));
            memset(t,0,sizeof(Task));
            t->type = T_DOWNLOAD;
            strncpy(t->username, username, sizeof(t->username)-1);
            strncpy(t->filename, filename, sizeof(t->filename)-1);
            t->client_sock = sock;
            pthread_mutex_init(&t->resp_mtx, NULL);
            pthread_cond_init(&t->resp_cond, NULL);
            t->done = 0;

            queue_push(&taskQueue, t);

            // wait for worker
            pthread_mutex_lock(&t->resp_mtx);
            while (!t->done) pthread_cond_wait(&t->resp_cond, &t->resp_mtx);
            pthread_mutex_unlock(&t->resp_mtx);

            if (t->status == 0) {
                // open tmp file and send size + bytes
                struct stat st;
                if (stat(t->tmp_path, &st) == 0) {
                    char header[64];
                    snprintf(header, sizeof(header), "%zu\n", (size_t)st.st_size);
                    send(sock, header, strlen(header), 0);
                    FILE *f = fopen(t->tmp_path, "rb");
                    if (f) {
                        char buf3[8192];
                        size_t r;
                        while ((r = fread(buf3,1,sizeof(buf3), f)) > 0) {
                            send(sock, buf3, r, 0);
                        }
                        fclose(f);
                    }
                    unlink(t->tmp_path);
                } else {
                    send_line(sock, "DOWNLOAD ERROR: cannot stat tmp\n");
                }
            } else {
                char tmp[512];
                snprintf(tmp, sizeof(tmp), "DOWNLOAD ERROR: %s\n", t->errmsg);
                send_line(sock, tmp);
            }
            pthread_mutex_destroy(&t->resp_mtx);
            pthread_cond_destroy(&t->resp_cond);
            free(t);
        }
        else if (strcasecmp(cmd, "DELETE") == 0) {
            if (ret < 3) { send_line(sock, "ERROR: DELETE requires username and filename\n"); continue; }
            Task *t = malloc(sizeof(Task));
            memset(t,0,sizeof(Task));
            t->type = T_DELETE;
            strncpy(t->username, username, sizeof(t->username)-1);
            strncpy(t->filename, filename, sizeof(t->filename)-1);
            t->client_sock = sock;
            pthread_mutex_init(&t->resp_mtx, NULL);
            pthread_cond_init(&t->resp_cond, NULL);
            t->done = 0;

            queue_push(&taskQueue, t);

            pthread_mutex_lock(&t->resp_mtx);
            while (!t->done) pthread_cond_wait(&t->resp_cond, &t->resp_mtx);
            pthread_mutex_unlock(&t->resp_mtx);

            if (t->status == 0) send_line(sock, "DELETE OK\n");
            else {
                char tmp[512];
                snprintf(tmp, sizeof(tmp), "DELETE ERROR: %s\n", t->errmsg);
                send_line(sock, tmp);
            }
            pthread_mutex_destroy(&t->resp_mtx);
            pthread_cond_destroy(&t->resp_cond);
            free(t);
        }
        else if (strcasecmp(cmd, "LIST") == 0) {
            if (ret < 2) { send_line(sock, "ERROR: LIST requires username\n"); continue; }
            // here username is in cmd args (we read as first arg)
            // if second arg present it was parsed; adjust
            Task *t = malloc(sizeof(Task));
            memset(t,0,sizeof(Task));
            t->type = T_LIST;
            strncpy(t->username, username, sizeof(t->username)-1);
            t->client_sock = sock;
            pthread_mutex_init(&t->resp_mtx, NULL);
            pthread_cond_init(&t->resp_cond, NULL);
            t->done = 0;

            queue_push(&taskQueue, t);

            pthread_mutex_lock(&t->resp_mtx);
            while (!t->done) pthread_cond_wait(&t->resp_cond, &t->resp_mtx);
            pthread_mutex_unlock(&t->resp_mtx);

            if (t->status == 0) {
                // send list file contents
                FILE *f = fopen(t->tmp_path, "r");
                if (f) {
                    char lbuf[1024];
                    while (fgets(lbuf, sizeof(lbuf), f)) {
                        send(sock, lbuf, strlen(lbuf), 0);
                    }
                    fclose(f);
                    unlink(t->tmp_path);
                } else {
                    send_line(sock, "LIST OK (empty)\n");
                }
            } else {
                char tmp[512];
                snprintf(tmp, sizeof(tmp), "LIST ERROR: %s\n", t->errmsg);
                send_line(sock, tmp);
            }
            pthread_mutex_destroy(&t->resp_mtx);
            pthread_cond_destroy(&t->resp_cond);
            free(t);
        }
        else if (strcasecmp(cmd, "QUIT") == 0 || strcasecmp(cmd, "EXIT")==0) {
            send_line(sock, "BYE\n");
            break;
        }
        else {
            send_line(sock, "Unknown command\n");
        }
    }
client_end:
    close(sock);
}
