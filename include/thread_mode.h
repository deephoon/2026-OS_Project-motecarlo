#ifndef THREAD_MODE_H
#define THREAD_MODE_H

#include "config.h"
#include "metrics.h"
#include "result.h"

int run_thread_mode(const Config *cfg, Result *out, double *elapsed_out);
int run_thread_mode_metrics(const Config *cfg, Result *out, StageMetrics *metrics);

#endif
