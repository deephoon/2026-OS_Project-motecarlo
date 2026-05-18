#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include "config.h"
#include "result.h"

typedef struct {
    int valid;
    long hist_sum;
    double collision_probability;
    double risk_ratio[RISK_BUCKETS];
} PostSummary;

int postprocess_finalize(Result *result, long expected_trials, PostSummary *summary);
void postprocess_run_extra_work(const Config *cfg, const Result *result);

#endif
