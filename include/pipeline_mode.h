#ifndef PIPELINE_MODE_H
#define PIPELINE_MODE_H

#include "config.h"
#include "metrics.h"
#include "result.h"

int run_pipeline_mode(const Config *cfg, Result *out, StageMetrics *metrics);

#endif
