#ifndef SEQUENTIAL_H
#define SEQUENTIAL_H

#include "config.h"
#include "result.h"

/* Runs the sequential baseline and records wall-clock elapsed time. */
int run_sequential(const Config *cfg, Result *out, double *elapsed_out);

#endif
