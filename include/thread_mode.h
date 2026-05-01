#ifndef THREAD_MODE_H
#define THREAD_MODE_H

#include "config.h"
#include "result.h"

/* Runs the pthread implementation with the configured synchronization mode. */
int run_thread_mode(const Config *cfg, Result *out, double *elapsed_out);

#endif
