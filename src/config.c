#include "config.h"

void config_set_defaults(Config *cfg)
{
    if (cfg == 0) {
        return;
    }
    cfg->mode = MODE_THREAD;
    cfg->sync_mode = SYNC_REDUCE;
    cfg->schedule_mode = SCHEDULE_STATIC;
    cfg->merge_mode = MERGE_FINAL;
    cfg->ipc_mode = IPC_PIPE;
    cfg->workload_mode = WORKLOAD_UNIFORM;
    cfg->scaling_mode = SCALING_STRONG;
    cfg->trials = 100000;
    cfg->work_iters = 1000000000LL;
    cfg->time_steps = 50;
    cfg->threads = 4;
    cfg->processes = 2;
    cfg->batch_size = 1000;
    cfg->queue_size = 1024;
    cfg->skew_factor = 8;
    cfg->affinity_enabled = 0;
    cfg->core_count = 0;
    cfg->pre_work = 0;
    cfg->post_work = 0;
    cfg->enable_pipeline = 1;
    cfg->metrics_detail = 1;
    cfg->seed = 42u;
    cfg->verbose = 0;
}

int config_validate(const Config *cfg)
{
    if (cfg == 0) {
        return 0;
    }
    if (cfg->mode < MODE_SEQ || cfg->mode > MODE_IDEAL) {
        return 0;
    }
    if (cfg->sync_mode < SYNC_NOSYNC || cfg->sync_mode > SYNC_REDUCE) {
        return 0;
    }
    if (cfg->schedule_mode < SCHEDULE_STATIC || cfg->schedule_mode > SCHEDULE_QUEUE) {
        return 0;
    }
    if (cfg->merge_mode < MERGE_FINAL || cfg->merge_mode > MERGE_INTERACTIVE) {
        return 0;
    }
    if (cfg->ipc_mode < IPC_PIPE || cfg->ipc_mode > IPC_SHM) {
        return 0;
    }
    if (cfg->workload_mode < WORKLOAD_UNIFORM ||
        cfg->workload_mode > WORKLOAD_SKEWED) {
        return 0;
    }
    if (cfg->scaling_mode < SCALING_STRONG ||
        cfg->scaling_mode > SCALING_UTILIZATION) {
        return 0;
    }
    if (cfg->trials <= 0 || cfg->time_steps <= 0 || cfg->threads <= 0 ||
        cfg->processes <= 0 || cfg->batch_size <= 0 || cfg->queue_size <= 0 ||
        cfg->skew_factor <= 0 || cfg->work_iters <= 0 ||
        cfg->pre_work < 0 || cfg->post_work < 0 || cfg->core_count < 0) {
        return 0;
    }
    return 1;
}
