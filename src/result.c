#include "result.h"

#include "sync.h"

#include <stdio.h>

void result_init(Result *r)
{
    if (r == 0) {
        return;
    }

    r->total_trials = 0;
    r->collision_count = 0;
    r->checksum = 0;
    for (int i = 0; i < RISK_BUCKETS; ++i) {
        r->histogram[i] = 0;
    }
}

void result_add_trial(Result *r, RiskLevel risk, int collided)
{
    if (r == 0 || risk < 0 || risk >= RISK_BUCKETS) {
        return;
    }

    r->total_trials += 1;
    if (collided) {
        r->collision_count += 1;
    }
    r->histogram[risk] += 1;
}

void result_merge(Result *dst, const Result *src)
{
    if (dst == 0 || src == 0) {
        return;
    }

    dst->total_trials += src->total_trials;
    dst->collision_count += src->collision_count;
    for (int i = 0; i < RISK_BUCKETS; ++i) {
        dst->histogram[i] += src->histogram[i];
    }
}

long result_hist_sum(const Result *r)
{
    long sum = 0;

    if (r == 0) {
        return 0;
    }

    for (int i = 0; i < RISK_BUCKETS; ++i) {
        sum += r->histogram[i];
    }
    return sum;
}

unsigned long result_compute_checksum(const Result *r)
{
    unsigned long hash = 1469598103934665603ul;

    if (r == 0) {
        return 0;
    }

    hash ^= (unsigned long)r->total_trials;
    hash *= 1099511628211ul;
    hash ^= (unsigned long)r->collision_count;
    hash *= 1099511628211ul;

    for (int i = 0; i < RISK_BUCKETS; ++i) {
        hash ^= (unsigned long)r->histogram[i] + (unsigned long)(i + 1);
        hash *= 1099511628211ul;
    }

    return hash;
}

int result_validate(const Result *r, long expected_trials)
{
    if (r == 0 || expected_trials <= 0) {
        return 0;
    }

    if (r->total_trials != expected_trials) {
        return 0;
    }
    if (result_hist_sum(r) != expected_trials) {
        return 0;
    }
    if (r->collision_count < 0 || r->collision_count > expected_trials) {
        return 0;
    }
    return 1;
}

void result_print_csv_header(void)
{
    puts("mode,sync,threads,trials,steps,time_sec,speedup,total_trials,"
         "collision_count,hist_sum,checksum,valid");
}

void result_print_csv_row(const Config *cfg, double time_sec, double speedup,
                          const Result *r, int valid)
{
    printf("%s,%s,%d,%ld,%d,%.6f,%.6f,%ld,%ld,%ld,%lu,%d\n",
           run_mode_to_string(cfg->mode),
           sync_mode_to_string(cfg->sync_mode),
           cfg->threads,
           cfg->trials,
           cfg->time_steps,
           time_sec,
           speedup,
           r->total_trials,
           r->collision_count,
           result_hist_sum(r),
           r->checksum,
           valid);
}
