#include "cli.h"
#include "config.h"
#include "hybrid_mode.h"
#include "ideal_benchmark.h"
#include "metrics.h"
#include "pipeline_mode.h"
#include "process_mode.h"
#include "result.h"
#include "sequential.h"
#include "sync.h"
#include "thread_mode.h"

#include <stdio.h>

static void print_ideal_header(void)
{
    puts("mode,scaling,threads,work_iters,affinity,time_total,checksum,valid");
}

static void print_ideal_row(const Config *cfg, const IdealResult *result)
{
    int valid = result != 0 && result->checksum != 0ULL;
    printf("%s,%s,%d,%lld,%s,%.9f,%llu,%d\n",
           run_mode_to_string(cfg->mode),
           scaling_mode_to_string(cfg->scaling_mode),
           cfg->threads,
           cfg->work_iters,
           affinity_to_string(cfg->affinity_enabled),
           result->time_total,
           result->checksum,
           valid);
}

static void print_final_header(void)
{
    puts("mode,schedule,merge,sync,processes,threads,trials,steps,batch_size,"
         "queue_size,ipc,workload,skew_factor,pre_work,post_work,"
         "time_total,time_pre,time_compute,time_sync,time_ipc,time_merge,"
         "time_post,speedup,efficiency,sequential_fraction_estimate,"
         "compute_ratio,sync_overhead_ratio,ipc_overhead_ratio,merge_overhead_ratio,"
         "throughput_batches_per_sec,total_trials,collision_count,hist_sum,"
         "checksum,valid,lock_wait_count,cond_wait_count,queue_push_count,"
         "queue_pop_count,ipc_write_count,ipc_read_count,ipc_bytes,notes,"
         "profile,inner_work");
}

static void print_final_row(const Config *cfg, const StageMetrics *m,
                            const Result *r, int valid, const char *notes)
{
    double total = metrics_total(m);
    double speedup = 0.0;
    double efficiency = 0.0;
    int workers = cfg->mode == MODE_PROCESS ? cfg->processes :
                  cfg->mode == MODE_HYBRID ? cfg->processes * cfg->threads :
                  cfg->threads;
    (void)speedup;
    (void)efficiency;
    printf("%s,%s,%s,%s,%d,%d,%ld,%d,%d,%d,%s,%s,%d,%d,%d,"
           "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,"
           "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%ld,%ld,%ld,%lu,%d,"
           "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%s,%s,%lld\n",
           run_mode_to_string(cfg->mode),
           schedule_mode_to_string(cfg->schedule_mode),
           merge_mode_to_string(cfg->merge_mode),
           sync_mode_to_string(cfg->sync_mode),
           cfg->processes,
           cfg->threads,
           cfg->trials,
           cfg->time_steps,
           cfg->batch_size,
           cfg->queue_size,
           ipc_mode_to_string(cfg->ipc_mode),
           workload_mode_to_string(cfg->workload_mode),
           cfg->skew_factor,
           cfg->pre_work,
           cfg->post_work,
           total,
           m->t_pre,
           m->t_compute,
           m->t_sync,
           m->t_ipc,
           m->t_merge,
           m->t_post,
           speedup,
           workers > 0 ? efficiency / (double)workers : 0.0,
           metrics_sequential_fraction_estimate(m),
           metrics_compute_ratio(m),
           metrics_sync_ratio(m),
           metrics_ipc_ratio(m),
           metrics_merge_ratio(m),
           metrics_throughput_batches(m),
           r->total_trials,
           r->collision_count,
           result_hist_sum(r),
           r->checksum,
           valid,
           m->lock_wait_count,
           m->cond_wait_count,
           m->queue_push_count,
           m->queue_pop_count,
           m->ipc_write_count,
           m->ipc_read_count,
           m->ipc_bytes,
           notes,
           profile_to_string(cfg->profile),
           cfg->inner_work);
}

static void print_verbose_summary(const Config *cfg, const Result *r,
                                  const StageMetrics *m, int valid)
{
    fprintf(stderr, "mode=%s schedule=%s merge=%s sync=%s ipc=%s workload=%s profile=%s\n",
            run_mode_to_string(cfg->mode),
            schedule_mode_to_string(cfg->schedule_mode),
            merge_mode_to_string(cfg->merge_mode),
            sync_mode_to_string(cfg->sync_mode),
            ipc_mode_to_string(cfg->ipc_mode),
            workload_mode_to_string(cfg->workload_mode),
            profile_to_string(cfg->profile));
    fprintf(stderr, "total=%.6f pre=%.6f compute=%.6f sync=%.6f ipc=%.6f merge=%.6f post=%.6f\n",
            metrics_total(m), m->t_pre, m->t_compute, m->t_sync,
            m->t_ipc, m->t_merge, m->t_post);
    fprintf(stderr, "trials=%ld hist_sum=%ld collisions=%ld checksum=%lu valid=%d\n",
            r->total_trials, result_hist_sum(r), r->collision_count,
            r->checksum, valid);
}

int main(int argc, char **argv)
{
    Config cfg;
    Result result;
    StageMetrics metrics;
    int rc = 0;
    int valid;
    const char *notes = "ok";
    IdealResult ideal_result;

    config_set_defaults(&cfg);
    if (cli_parse_args(argc, argv, &cfg) != 0) {
        cli_print_usage(stderr, argv[0]);
        return 2;
    }
    config_apply_profile_defaults(&cfg);

    if (cfg.mode == MODE_IDEAL) {
        rc = run_ideal_benchmark(&cfg, &ideal_result, &metrics);
        if (rc != 0) {
            fprintf(stderr, "ideal benchmark failed\n");
            return 1;
        }
        print_ideal_header();
        print_ideal_row(&cfg, &ideal_result);
        return ideal_result.checksum != 0ULL ? 0 : 1;
    }

    switch (cfg.mode) {
    case MODE_SEQ:
        rc = run_sequential_metrics(&cfg, &result, &metrics);
        break;
    case MODE_THREAD:
        if (cfg.schedule_mode == SCHEDULE_QUEUE) {
            rc = run_pipeline_mode(&cfg, &result, &metrics);
        } else {
            rc = run_thread_mode_metrics(&cfg, &result, &metrics);
        }
        break;
    case MODE_PIPELINE:
        cfg.schedule_mode = SCHEDULE_QUEUE;
        rc = run_pipeline_mode(&cfg, &result, &metrics);
        break;
    case MODE_PROCESS:
        rc = run_process_mode(&cfg, &result, &metrics);
        break;
    case MODE_HYBRID:
        rc = run_hybrid_mode(&cfg, &result, &metrics);
        break;
    case MODE_IDEAL:
        break;
    }

    if (rc != 0) {
        fprintf(stderr, "simulation failed\n");
        return 1;
    }

    valid = result_validate(&result, cfg.trials);
    if (cfg.metrics_detail) {
        print_final_header();
        print_final_row(&cfg, &metrics, &result, valid, notes);
    } else {
        result_print_csv_row(&cfg, metrics_total(&metrics), 1.0, &result, valid);
    }

    if (cfg.verbose) {
        print_verbose_summary(&cfg, &result, &metrics, valid);
    }
    return 0;
}
