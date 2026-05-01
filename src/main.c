#include "cli.h"
#include "config.h"
#include "result.h"
#include "sequential.h"
#include "sync.h"
#include "thread_mode.h"

#include <stdio.h>

static void print_verbose_summary(const Config *cfg, const Result *result,
                                  double time_sec, int valid)
{
    fprintf(stderr, "mode=%s sync=%s threads=%d trials=%ld steps=%d\n",
            run_mode_to_string(cfg->mode),
            sync_mode_to_string(cfg->sync_mode),
            cfg->threads,
            cfg->trials,
            cfg->time_steps);
    fprintf(stderr, "time_sec=%.6f total_trials=%ld hist_sum=%ld valid=%d\n",
            time_sec,
            result->total_trials,
            result_hist_sum(result),
            valid);
    fprintf(stderr, "collisions=%ld checksum=%lu\n",
            result->collision_count,
            result->checksum);
}

int main(int argc, char **argv)
{
    Config cfg;
    Result result;
    double time_sec = 0.0;
    double speedup = 1.0;
    int valid;
    int rc;

    config_set_defaults(&cfg);
    if (cli_parse_args(argc, argv, &cfg) != 0) {
        cli_print_usage(stderr, argv[0]);
        return 2;
    }

    result_init(&result);
    if (cfg.mode == MODE_SEQ) {
        rc = run_sequential(&cfg, &result, &time_sec);
    } else {
        rc = run_thread_mode(&cfg, &result, &time_sec);
    }

    if (rc != 0) {
        fprintf(stderr, "simulation failed\n");
        return 1;
    }

    valid = result_validate(&result, cfg.trials);
    result_print_csv_row(&cfg, time_sec, speedup, &result, valid);

    if (cfg.verbose) {
        print_verbose_summary(&cfg, &result, time_sec, valid);
    }

    return 0;
}
