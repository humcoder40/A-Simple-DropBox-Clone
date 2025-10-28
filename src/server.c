#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>

#include "metadata.h"
#include "queue.h"

#define PORT 9000
#define STORAGE_DIR "storage"

void start_worker_pool(void);

extern Queue clientQueue;
extern Queue taskQueue;

void handle_client(int sock) {
    char buf[1024];
    char logged_in_user[64] = "";

    const char *welcome_msg =
        "Welcome to MiniDropBox!\nCommands:\n"
        "SIGNUP user pass\nLOGIN user pass\n"
        "UPLOAD <username> <filename>\nDOWNLOAD <username> <filename>\n"
        "DELETE <username> <filename>\nLIST <username>\nQUIT\n";

    send(sock, welcome_msg, strlen(welcome_msg), 0);

    while (1) {
        memset(buf, 0, sizeof(buf));
        ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        buf[strcspn(buf, "\r\n")] = 0;

        // ---------- SIGNUP ----------
        if (strncasecmp(buf, "SIGNUP", 6) == 0) {
            char user[64] = {0}, pass[64] = {0};
            if (sscanf(buf + 6, "%63s %63s", user, pass) == 2) {
                signup_user(user, pass);
                send(sock, "SIGNUP OK\n", 10, 0);
            } else {
                send(sock, "Usage: SIGNUP user pass\n", 24, 0);
            }
            continue;
        }

        // ---------- LOGIN ----------
        if (strncasecmp(buf, "LOGIN", 5) == 0) {
            char user[64] = {0}, pass[64] = {0};
            if (sscanf(buf + 5, "%63s %63s", user, pass) == 2 &&
                user_exists(user, pass)) {
                strcpy(logged_in_user, user);
                send(sock, "LOGIN OK\n", 9, 0);
            } else {
                send(sock, "INVALID CREDENTIALS\n", 20, 0);
            }
            continue;
        }

        if (strlen(logged_in_user) == 0) {
            send(sock, "PLEASE LOGIN FIRST\n", 19, 0);
            continue;
        }

        // ---------- UPLOAD ----------
        if (strncasecmp(buf, "UPLOAD", 6) == 0) {
            char user[64] = {0}, filename[64] = {0};
            if (sscanf(buf + 6, "%63s %63s", user, filename) != 2 ||
                strcmp(user, logged_in_user) != 0) {
                send(sock, "PERMISSION DENIED\n", 18, 0);
                continue;
            }

            char userdir[256];
            snprintf(userdir, sizeof(userdir), "%s/%s", STORAGE_DIR, user);
            ensure_dir(userdir);

            char path[512];
            snprintf(path, sizeof(path), "%s/%s/%s", STORAGE_DIR, user, filename);
            FILE *f = fopen(path, "wb");
            if (!f) {
                send(sock, "UPLOAD FAIL\n", 12, 0);
                continue;
            }

            send(sock, "READY_FOR_DATA\n", 15, 0);

            char linebuf[1024];
            size_t linepos = 0;
            while (1) {
                ssize_t n = recv(sock, buf, sizeof(buf), 0);
                if (n <= 0) break;

                for (ssize_t i = 0; i < n; ++i) {
                    if (linepos < sizeof(linebuf) - 1) linebuf[linepos++] = buf[i];
                    if (buf[i] == '\n') {
                        linebuf[linepos] = 0;
                        if (linepos >= 2 && linebuf[linepos-2] == '\r') linebuf[linepos-2] = 0;

                        if (strcmp(linebuf, "END") == 0) goto done_upload;

                        fwrite(linebuf, 1, strlen(linebuf), f);
                        fwrite("\n", 1, 1, f);
                        linepos = 0;
                    }
                }
            }

        done_upload:
            fclose(f);
            send(sock, "UPLOAD OK\n", 10, 0);
            continue;
        }

        // ---------- DOWNLOAD ----------
        if (strncasecmp(buf, "DOWNLOAD", 8) == 0) {
            char user[64] = {0}, filename[64] = {0};
            if (sscanf(buf + 8, "%63s %63s", user, filename) != 2 ||
                strcmp(user, logged_in_user) != 0) {
                send(sock, "PERMISSION DENIED\n", 18, 0);
                continue;
            }

            char path[512];
            snprintf(path, sizeof(path), "%s/%s/%s", STORAGE_DIR, user, filename);
            FILE *f = fopen(path, "rb");
            if (!f) {
                send(sock, "DOWNLOAD FAIL\n", 14, 0);
                continue;
            }
            while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
                send(sock, buf, n, 0);
            fclose(f);
            send(sock, "\nDOWNLOAD OK\n", 13, 0);
            continue;
        }

        // ---------- DELETE ----------
        if (strncasecmp(buf, "DELETE", 6) == 0) {
            char user[64] = {0}, filename[64] = {0};
            if (sscanf(buf + 6, "%63s %63s", user, filename) != 2 ||
                strcmp(user, logged_in_user) != 0) {
                send(sock, "PERMISSION DENIED\n", 18, 0);
                continue;
            }

            char path[512];
            snprintf(path, sizeof(path), "%s/%s/%s", STORAGE_DIR, user, filename);
            if (remove(path) == 0)
                send(sock, "DELETE OK\n", 10, 0);
            else
                send(sock, "DELETE FAIL\n", 12, 0);
            continue;
        }

        // ---------- LIST ----------
        if (strncasecmp(buf, "LIST", 4) == 0) {
            char user[64] = {0};
            if (sscanf(buf + 4, "%63s", user) != 1 ||
                strcmp(user, logged_in_user) != 0) {
                send(sock, "PERMISSION DENIED\n", 18, 0);
                continue;
            }

            char dirpath[512];
            snprintf(dirpath, sizeof(dirpath), "%s/%s", STORAGE_DIR, user);
            DIR *d = opendir(dirpath);
            if (!d) {
                send(sock, "LIST FAIL\n", 10, 0);
                continue;
            }
            struct dirent *de;
            while ((de = readdir(d))) {
                if (strcmp(de->d_name, ".") && strcmp(de->d_name, "..")) {
                    char line[256];
                    snprintf(line, sizeof(line), "%s\n", de->d_name);
                    send(sock, line, strlen(line), 0);
                }
            }
            closedir(d);
            send(sock, "LIST OK\n", 8, 0);
            continue;
        }

        // ---------- QUIT ----------
        if (strncasecmp(buf, "QUIT", 4) == 0) {
            send(sock, "BYE\n", 4, 0);
            break;
        }

        // ---------- UNKNOWN ----------
        send(sock, "UNKNOWN COMMAND\n", 16, 0);
    }

    close(sock);
}

void *client_thread(void *arg) {
    int sock = *((int *)arg);
    free(arg);
    handle_client(sock);
    return NULL;
}

int main() {
    ensure_dir(STORAGE_DIR);
    ensure_dir("metadata");

    queue_init(&clientQueue, 128);
    queue_init(&taskQueue, 256);

    start_worker_pool();

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(server_fd); exit(1);
    }

    if (listen(server_fd, 128) < 0) {
        perror("listen"); close(server_fd); exit(1);
    }

    printf("Server running on port %d...\n", PORT);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) { perror("accept"); continue; }

        int *pclient = malloc(sizeof(int));
        if (!pclient) { close(client_fd); continue; }
        *pclient = client_fd;

        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&tid, &attr, client_thread, pclient) != 0) {
            perror("pthread_create"); free(pclient); close(client_fd);
        }
        pthread_attr_destroy(&attr);
    }

    close(server_fd);
    return 0;
}
