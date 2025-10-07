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
#include "metadata.h"

#define PORT 9000
#define STORAGE_DIR "storage"

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
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        buf[strcspn(buf, "\r\n")] = 0; // remove newline

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

        // ---------- must be logged in ----------
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

            char path[256];
            snprintf(path, sizeof(path), "%s/%s/%s", STORAGE_DIR, user, filename);
            FILE *f = fopen(path, "wb");
            if (!f) {
                send(sock, "UPLOAD FAIL\n", 12, 0);
                continue;
            }

            send(sock, "READY_FOR_DATA\n", 15, 0);
            while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
                if (strcmp(buf, "END\n") == 0) break;
                fwrite(buf, 1, n, f);
                if (strstr(buf, "\n")) break;
            }
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

            char path[256];
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

            char path[256];
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

            char dirpath[256];
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

/* ---------- MAIN ---------- */
int main() {
    ensure_dir(STORAGE_DIR);
    ensure_dir("metadata");

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 5);
    printf("Server running on port %d...\n", PORT);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;
        if (fork() == 0) {
            close(server_fd);
            handle_client(client_fd);
            exit(0);
        }
        close(client_fd);
    }
}
