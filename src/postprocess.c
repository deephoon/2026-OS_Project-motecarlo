#include "postprocess.h"

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
