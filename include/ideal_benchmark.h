#ifndef IDEAL_BENCHMARK_H
#define IDEAL_BENCHMARK_H

#include "config.h"
#include "metrics.h"

typedef struct {
    double time_total;
    unsigned long long checksum;
} IdealResult;

int run_ideal_benchmark(const Config *cfg, IdealResult *out, Metrics *metrics);

#endif
