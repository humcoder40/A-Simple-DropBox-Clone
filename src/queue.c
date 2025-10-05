#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

void queue_init(Queue *q, int capacity) {
    q->items = malloc(sizeof(void*) * capacity);
    q->front = q->rear = q->count = 0;
    q->capacity = capacity;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_push(Queue *q, void *item) {
    pthread_mutex_lock(&q->lock);
    while (q->count == q->capacity)
        pthread_cond_wait(&q->not_full, &q->lock);

    q->items[q->rear] = item;
    q->rear = (q->rear + 1) % q->capacity;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

void *queue_pop(Queue *q) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0)
        pthread_cond_wait(&q->not_empty, &q->lock);

    void *item = q->items[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return item;
}

void queue_destroy(Queue *q) {
    free(q->items);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}
