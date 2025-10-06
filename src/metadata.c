#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include "common.h"

// Simple user struct and global list (fine for phase1)
typedef struct User {
    char username[64];
    size_t used_bytes;
    size_t quota_bytes;
    pthread_mutex_t lock;
    struct User *next;
} User;

static User *users_head = NULL;
static pthread_mutex_t users_mtx = PTHREAD_MUTEX_INITIALIZER;

User* find_user(const char *username) {
    pthread_mutex_lock(&users_mtx);
    User *u = users_head;
    while (u) {
        if (strcmp(u->username, username) == 0) {
            pthread_mutex_unlock(&users_mtx);
            return u;
        }
        u = u->next;
    }
    pthread_mutex_unlock(&users_mtx);
    return NULL;
}

User* create_user_if_missing(const char *username) {
    User *u = find_user(username);
    if (u) return u;

    u = malloc(sizeof(User));
    strncpy(u->username, username, sizeof(u->username)-1);
    u->username[sizeof(u->username)-1] = '\0';
    u->used_bytes = 0;
    u->quota_bytes = USER_QUOTA_BYTES;
    pthread_mutex_init(&u->lock, NULL);
    u->next = NULL;

    pthread_mutex_lock(&users_mtx);
    u->next = users_head;
    users_head = u;
    pthread_mutex_unlock(&users_mtx);

    // ensure user storage directory exists
    char dir[PATH_MAX_LEN];
    snprintf(dir, sizeof(dir), "%s/%s", STORAGE_ROOT, username);
    mkdir(STORAGE_ROOT, 0755); // ensure parent exists (ok if exists)
    mkdir(dir, 0755);

    return u;
}

int add_usage(const char *username, size_t bytes) {
    User *u = create_user_if_missing(username);
    pthread_mutex_lock(&u->lock);
    if (u->used_bytes + bytes > u->quota_bytes) {
        pthread_mutex_unlock(&u->lock);
        return -1; // exceed
    }
    u->used_bytes += bytes;
    pthread_mutex_unlock(&u->lock);
    return 0;
}

void reduce_usage(const char *username, size_t bytes) {
    User *u = create_user_if_missing(username);
    pthread_mutex_lock(&u->lock);
    if (bytes > u->used_bytes) u->used_bytes = 0;
    else u->used_bytes -= bytes;
    pthread_mutex_unlock(&u->lock);
}

size_t get_user_usage(const char *username) {
    User *u = find_user(username);
    if (!u) return 0;
    pthread_mutex_lock(&u->lock);
    size_t v = u->used_bytes;
    pthread_mutex_unlock(&u->lock);
    return v;
}
