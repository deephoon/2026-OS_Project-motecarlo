#ifndef TASK_BATCH_H
#define TASK_BATCH_H

typedef struct {
    int batch_id;
    long start_idx;
    long end_idx;
    unsigned int base_seed;
    int time_steps;
    int difficulty_level;
} TaskBatch;

#endif
