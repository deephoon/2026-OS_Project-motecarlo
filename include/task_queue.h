#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include "task_batch.h"
#include "metrics.h"

#include <pthread.h>

typedef struct {
    TaskBatch *buffer;
    int capacity;
    int head;
    int tail;
    int count;
    int closed;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} TaskQueue;

int task_queue_init(TaskQueue *q, int capacity);
void task_queue_destroy(TaskQueue *q);
int task_queue_push(TaskQueue *q, TaskBatch item, StageMetrics *metrics,
                    pthread_mutex_t *metrics_mutex);
int task_queue_pop(TaskQueue *q, TaskBatch *out, StageMetrics *metrics,
                   pthread_mutex_t *metrics_mutex);
void task_queue_close(TaskQueue *q);

#endif
