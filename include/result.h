#ifndef RESULT_H
#define RESULT_H

#include "config.h"

typedef enum {
    RISK_SAFE = 0,
    RISK_LOW = 1,
    RISK_MEDIUM = 2,
    RISK_HIGH = 3,
    RISK_COLLISION = 4,
    RISK_BUCKETS = 5
} RiskLevel;

typedef struct {
    long total_trials;
    long collision_count;
    long histogram[RISK_BUCKETS];
    unsigned long checksum;
} Result;

/* Initializes all counters in a Result object. */
void result_init(Result *r);

/* Adds one classified trial result to an aggregate. */
void result_add_trial(Result *r, RiskLevel risk, int collided);

/* Merges a per-thread local aggregate into a destination aggregate. */
void result_merge(Result *dst, const Result *src);

/* Returns the sum across all risk buckets. */
long result_hist_sum(const Result *r);

/* Computes a stable checksum from aggregate counters for result comparison. */
unsigned long result_compute_checksum(const Result *r);

/* Validates that total and histogram counters match the expected trial count. */
int result_validate(const Result *r, long expected_trials);

/* Prints the CSV header used by scripts and manual runs. */
void result_print_csv_header(void);

/* Prints one CSV row for a completed run. */
void result_print_csv_row(const Config *cfg, double time_sec, double speedup,
                          const Result *r, int valid);

#endif
