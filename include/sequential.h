#ifndef SEQUENTIAL_H
#define SEQUENTIAL_H

#include "config.h"
#include "metrics.h"
#include "result.h"

int run_sequential(const Config *cfg, Result *out, double *elapsed_out);
int run_sequential_metrics(const Config *cfg, Result *out, StageMetrics *metrics);

#endif
