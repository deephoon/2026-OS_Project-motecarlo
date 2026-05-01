#include "config.h"

void config_set_defaults(Config *cfg)
{
    if (cfg == 0) {
        return;
    }

    cfg->mode = MODE_SEQ;
    cfg->sync_mode = SYNC_REDUCE;
    cfg->trials = 100000;
    cfg->time_steps = 50;
    cfg->threads = 4;
    cfg->seed = 42u;
    cfg->verbose = 0;
}

int config_validate(const Config *cfg)
{
    if (cfg == 0) {
        return 0;
    }
    if (cfg->mode != MODE_SEQ && cfg->mode != MODE_THREAD) {
        return 0;
    }
    if (cfg->sync_mode != SYNC_NOSYNC &&
        cfg->sync_mode != SYNC_MUTEX &&
        cfg->sync_mode != SYNC_REDUCE) {
        return 0;
    }
    if (cfg->trials <= 0 || cfg->time_steps <= 0 || cfg->threads <= 0) {
        return 0;
    }
    return 1;
}
