#include "merge_queue.h"

#include <stdlib.h>

int merge_queue_init(MergeQueue *q, int capacity)
{
    if (q == 0 || capacity <= 0) {
        return -1;
    }
    q->buffer = (PartialResult *)calloc((size_t)capacity, sizeof(*q->buffer));
    if (q->buffer == 0) {
        return -1;
    }
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->closed = 0;
    if (pthread_mutex_init(&q->mutex, 0) != 0 ||
        pthread_cond_init(&q->not_empty, 0) != 0 ||
        pthread_cond_init(&q->not_full, 0) != 0) {
        free(q->buffer);
        q->buffer = 0;
        return -1;
    }
    return 0;
}

void merge_queue_destroy(MergeQueue *q)
{
    if (q == 0) return;
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
    pthread_mutex_destroy(&q->mutex);
    free(q->buffer);
    q->buffer = 0;
}

int merge_queue_push(MergeQueue *q, PartialResult item)
{
    if (q == 0) return 0;
    pthread_mutex_lock(&q->mutex);
    while (!q->closed && q->count == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    if (q->closed) {
        pthread_mutex_unlock(&q->mutex);
        return 0;
    }
    q->buffer[q->tail] = item;
    q->tail = (q->tail + 1) % q->capacity;
    q->count += 1;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return 1;
}

int merge_queue_pop(MergeQueue *q, PartialResult *out)
{
    if (q == 0 || out == 0) return 0;
    pthread_mutex_lock(&q->mutex);
    while (!q->closed && q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }
    if (q->count == 0 && q->closed) {
        pthread_mutex_unlock(&q->mutex);
        return 0;
    }
    *out = q->buffer[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count -= 1;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return 1;
}

void merge_queue_close(MergeQueue *q)
{
    if (q == 0) return;
    pthread_mutex_lock(&q->mutex);
    q->closed = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
}
