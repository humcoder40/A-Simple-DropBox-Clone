#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

typedef struct {
    void **items;
    int front, rear, count, capacity;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} Queue;

void queue_init(Queue *q, int capacity);
void queue_push(Queue *q, void *item);
void *queue_pop(Queue *q);
void queue_destroy(Queue *q);

#endif
