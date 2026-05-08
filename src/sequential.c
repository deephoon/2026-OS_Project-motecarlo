#include "sequential.h"

#include "metrics.h"
#include "postprocess.h"
#include "simulation.h"

int run_sequential_metrics(const Config *cfg, Result *out, StageMetrics *metrics)
{
    double start;
    double end;
    PostSummary summary;

    if (cfg == 0 || out == 0 || metrics == 0 ||
        cfg->trials <= 0 || cfg->time_steps <= 0) {
        return -1;
    }

    metrics_init(metrics);
    metrics->t_total_start = now_sec();
    result_init(out);
    start = now_sec();
    for (long i = 0; i < cfg->trials; ++i) {
        unsigned int trial_seed = simulation_seed_for_trial(cfg->seed, i);
        int collided = 0;
        RiskLevel risk = run_trial(&trial_seed, cfg->time_steps, &collided);
        result_add_trial(out, risk, collided);
    }
    end = now_sec();
    metrics->t_compute = elapsed_sec(start, end);

    start = now_sec();
    postprocess_finalize(out, cfg->trials, &summary);
    end = now_sec();
    metrics->t_post = elapsed_sec(start, end);
    metrics->t_total_end = now_sec();
    return 0;
}

int run_sequential(const Config *cfg, Result *out, double *elapsed_out)
{
    StageMetrics metrics;
    int rc = run_sequential_metrics(cfg, out, &metrics);
    if (elapsed_out != 0) {
        *elapsed_out = metrics_total(&metrics);
    }
    return rc;
}
