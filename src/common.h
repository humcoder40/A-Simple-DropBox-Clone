#ifndef COMMON_H
#define COMMON_H

#include "queue.h"
#include <pthread.h>

#define STORAGE_ROOT "./storage"
#define USER_QUOTA_BYTES (10 * 1024 * 1024) // 10 MB default quota
#define PATH_MAX_LEN 512

typedef struct User User;

typedef enum {
    T_UPLOAD, T_DOWNLOAD, T_DELETE, T_LIST
} TaskType;

typedef struct Task {
    TaskType type;
    char username[64];
    char filename[256];
    int client_sock;

    // temp file path (for UPLOAD: client wrote here; for DOWNLOAD: worker writes here)
    char tmp_path[PATH_MAX_LEN];
    // for DOWNLOAD worker will fill tmp_path which client reads and sends
    int status; // 0 success, <0 error
    char errmsg[256];

    // response sync
    pthread_mutex_t resp_mtx;
    pthread_cond_t resp_cond;
    int done; // 0 waiting, 1 done
} Task;

// Global queues (implemented in queue.c)
extern Queue clientQueue;
extern Queue taskQueue;

#endif

