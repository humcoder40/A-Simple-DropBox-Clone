#ifndef COMMON_H
#define COMMON_H

#include "queue.h"

typedef enum {
    T_UPLOAD, T_DOWNLOAD, T_DELETE, T_LIST
} TaskType;

typedef struct {
    TaskType type;
    char username[64];
    char filename[128];
    int client_sock;
} Task;

extern Queue clientQueue;
extern Queue taskQueue;

#endif
