#ifndef SIMULATION_H
#define SIMULATION_H

#include "result.h"

/* Builds a deterministic per-trial seed so parallel scheduling does not
 * change the random stream used by a given trial index. */
unsigned int simulation_seed_for_trial(unsigned int base_seed, long trial_index);

/* Runs one CPU-bound Monte Carlo trial and returns the classified risk level. */
RiskLevel run_trial(unsigned int *seed, int time_steps, int *collided_out);

#endif
