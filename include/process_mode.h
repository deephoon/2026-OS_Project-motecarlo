#ifndef PROCESS_MODE_H
#define PROCESS_MODE_H

#include "config.h"
#include "metrics.h"
#include "result.h"

int run_process_mode(const Config *cfg, Result *out, StageMetrics *metrics);

#endif
