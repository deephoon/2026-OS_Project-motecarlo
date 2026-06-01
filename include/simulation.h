#ifndef SIMULATION_H
#define SIMULATION_H

#include "config.h"
#include "result.h"
#include "task_batch.h"

unsigned int simulation_seed_for_trial(unsigned int base_seed, long trial_index);
RiskLevel run_trial(unsigned int *seed, int time_steps, int *collided_out);
RiskLevel run_trial_for_index(const Config *cfg, long trial_index,
                              int *collided_out);
void run_batch(const Config *cfg, const TaskBatch *batch, Result *out);

#endif
