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

void result_init(Result *r);
void result_add_trial(Result *r, RiskLevel risk, int collided);
void result_merge(Result *dst, const Result *src);
long result_hist_sum(const Result *r);
unsigned long result_compute_checksum(const Result *r);
int result_validate(const Result *r, long expected_trials);
void result_print_csv_header(void);
void result_print_csv_row(const Config *cfg, double time_sec, double speedup,
                          const Result *r, int valid);

#endif
