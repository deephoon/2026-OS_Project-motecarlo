#include "postprocess.h"

static volatile unsigned long postprocess_sink;

void postprocess_run_extra_work(const Config *cfg, const Result *result)
{
    unsigned long acc;
    long iterations;

    if (cfg == 0 || result == 0 || cfg->post_work <= 0) {
        return;
    }

    iterations = (long)cfg->post_work * (long)(RISK_BUCKETS + 2);
    acc = result->checksum ^ (unsigned long)result->total_trials;
    for (long i = 0; i < iterations; ++i) {
        acc += (unsigned long)result->histogram[i % RISK_BUCKETS] + (unsigned long)i;
        acc ^= acc << 7;
        acc ^= acc >> 11;
    }
    postprocess_sink = acc;
}

int postprocess_finalize(Result *result, long expected_trials, PostSummary *summary)
{
    long hist_sum;

    if (result == 0 || summary == 0) {
        return 0;
    }

    result->checksum = result_compute_checksum(result);
    hist_sum = result_hist_sum(result);
    summary->hist_sum = hist_sum;
    summary->valid = result_validate(result, expected_trials);
    summary->collision_probability =
        expected_trials > 0 ? (double)result->collision_count / (double)expected_trials : 0.0;

    for (int i = 0; i < RISK_BUCKETS; ++i) {
        summary->risk_ratio[i] =
            expected_trials > 0 ? (double)result->histogram[i] / (double)expected_trials : 0.0;
    }

    return summary->valid;
}
