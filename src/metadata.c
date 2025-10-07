#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define USERS_FILE "metadata/users.txt"
#define STORAGE_DIR "storage"

// ------------------------------------------------------------------
// Utility: ensure a directory exists (creates if missing)
// ------------------------------------------------------------------
void ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }
}

// ------------------------------------------------------------------
// Check if a username + password pair exists
// ------------------------------------------------------------------
int user_exists(const char *username, const char *password) {
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) return 0;

    char u[64], p[64];
    while (fscanf(f, "%63s %63s", u, p) == 2) {
        if (strcmp(u, username) == 0 && strcmp(p, password) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

// ------------------------------------------------------------------
// Register a new user; returns 1 on success, 0 if user already exists
// ------------------------------------------------------------------
int signup_user(const char *username, const char *password) {
    // check if already exists
    FILE *f = fopen(USERS_FILE, "r");
    if (f) {
        char u[64], p[64];
        while (fscanf(f, "%63s %63s", u, p) == 2) {
            if (strcmp(u, username) == 0) {
                fclose(f);
                return 0; // already exists
            }
        }
        fclose(f);
    }

    // append new user to metadata file
    f = fopen(USERS_FILE, "a");
    if (!f) return 0;
    fprintf(f, "%s %s\n", username, password);
    fclose(f);

    // create storage directory for this user
    ensure_dir(STORAGE_DIR);

    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), "%s/%s", STORAGE_DIR, username);
    ensure_dir(user_dir);

    return 1;
}

// ------------------------------------------------------------------
// Dummy quota tracking (Phase 2 will implement this)
// ------------------------------------------------------------------
void add_usage(const char *username, size_t amount) {
    (void)username;
    (void)amount;
    // TODO: implement disk quota accounting later
}

void reduce_usage(const char *username, size_t amount) {
    (void)username;
    (void)amount;
    // TODO: implement disk quota accounting later
}
