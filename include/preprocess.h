#ifndef PREPROCESS_H
#define PREPROCESS_H

#include "config.h"
#include "task_batch.h"

TaskBatch *create_batches(const Config *cfg, int *out_batch_count);
void free_batches(TaskBatch *batches);

#endif
