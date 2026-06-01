#ifndef MERGE_QUEUE_H
#define MERGE_QUEUE_H

#include "result.h"
#include "metrics.h"

#include <pthread.h>

typedef struct {
    int batch_id;
    Result result;
} PartialResult;

typedef struct {
    PartialResult *buffer;
    int capacity;
    int head;
    int tail;
    int count;
    int closed;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} MergeQueue;

int merge_queue_init(MergeQueue *q, int capacity);
void merge_queue_destroy(MergeQueue *q);
int merge_queue_push(MergeQueue *q, PartialResult item, StageMetrics *metrics,
                     pthread_mutex_t *metrics_mutex);
int merge_queue_pop(MergeQueue *q, PartialResult *out, StageMetrics *metrics,
                    pthread_mutex_t *metrics_mutex);
void merge_queue_close(MergeQueue *q);

#endif
