#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include "result.h"

typedef struct {
    int valid;
    long hist_sum;
    double collision_probability;
    double risk_ratio[RISK_BUCKETS];
} PostSummary;

int postprocess_finalize(Result *result, long expected_trials, PostSummary *summary);

#endif
