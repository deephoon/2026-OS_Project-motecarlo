#include "sequential.h"

#include "metrics.h"
#include "simulation.h"

int run_sequential(const Config *cfg, Result *out, double *elapsed_out)
{
    double start;
    double end;

    if (cfg == 0 || out == 0 || elapsed_out == 0 ||
        cfg->trials <= 0 || cfg->time_steps <= 0) {
        return -1;
    }

    result_init(out);
    start = now_sec();

    for (long i = 0; i < cfg->trials; ++i) {
        unsigned int trial_seed = simulation_seed_for_trial(cfg->seed, i);
        int collided = 0;
        RiskLevel risk = run_trial(&trial_seed, cfg->time_steps, &collided);
        result_add_trial(out, risk, collided);
    }

    end = now_sec();
    *elapsed_out = elapsed_sec(start, end);
    out->checksum = result_compute_checksum(out);
    return 0;
}
