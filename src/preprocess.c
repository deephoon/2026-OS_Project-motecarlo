#include "preprocess.h"

#include <stdlib.h>

TaskBatch *create_batches(const Config *cfg, int *out_batch_count)
{
    int count;
    TaskBatch *batches;

    if (cfg == 0 || out_batch_count == 0 || cfg->batch_size <= 0) {
        return 0;
    }

    count = (int)((cfg->trials + cfg->batch_size - 1) / cfg->batch_size);
    batches = (TaskBatch *)calloc((size_t)count, sizeof(*batches));
    if (batches == 0) {
        return 0;
    }

    for (int i = 0; i < count; ++i) {
        long start = (long)i * cfg->batch_size;
        long end = start + cfg->batch_size;
        if (end > cfg->trials) {
            end = cfg->trials;
        }
        batches[i].batch_id = i;
        batches[i].start_idx = start;
        batches[i].end_idx = end;
        batches[i].base_seed = cfg->seed ^ ((unsigned int)i * 1103515245u);
        batches[i].time_steps = cfg->time_steps;
        batches[i].difficulty_level = (int)(batches[i].base_seed % 3u);
    }

    *out_batch_count = count;
    return batches;
}

void free_batches(TaskBatch *batches)
{
    free(batches);
}
