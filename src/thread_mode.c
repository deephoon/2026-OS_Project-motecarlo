#include "thread_mode.h"

#include "metrics.h"
#include "postprocess.h"
#include "preprocess.h"
#include "simulation.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int thread_id;
    const Config *cfg;
    long start_idx;
    long end_idx;
    Result *global_result;
    Result local_result;
    pthread_mutex_t *result_mutex;
    StageMetrics *metrics;
} ThreadArg;

static void record_trial_result(ThreadArg *arg, RiskLevel risk, int collided)
{
    switch (arg->cfg->sync_mode) {
    case SYNC_REDUCE:
        result_add_trial(&arg->local_result, risk, collided);
        break;
    case SYNC_MUTEX:
    {
        double start = now_sec();
        double end;
        pthread_mutex_lock(arg->result_mutex);
        end = now_sec();
        if (arg->metrics != 0) {
            arg->metrics->t_sync += elapsed_sec(start, end);
            arg->metrics->lock_wait_count += 1;
        }
        result_add_trial(arg->global_result, risk, collided);
        pthread_mutex_unlock(arg->result_mutex);
        break;
    }
    case SYNC_NOSYNC:
        /* Deliberately unsafe: demonstrates lost updates in shared counters. */
        result_add_trial(arg->global_result, risk, collided);
        break;
    }
}

static void *thread_worker(void *arg_ptr)
{
    ThreadArg *arg = (ThreadArg *)arg_ptr;
    const Config *cfg = arg->cfg;
    result_init(&arg->local_result);
    for (long i = arg->start_idx; i < arg->end_idx; ++i) {
        int collided = 0;
        RiskLevel risk = run_trial_for_index(cfg, i, &collided);
        record_trial_result(arg, risk, collided);
    }
    return 0;
}

static void partition_work(long trials, int threads, int tid,
                           long *start_idx, long *end_idx)
{
    long base = trials / threads;
    long rem = trials % threads;
    long extra_before = tid < rem ? tid : rem;
    *start_idx = (long)tid * base + extra_before;
    *end_idx = *start_idx + base + (tid < rem ? 1 : 0);
}

int run_thread_mode_metrics(const Config *cfg, Result *out, StageMetrics *metrics)
{
    pthread_t *threads = 0;
    ThreadArg *args = 0;
    pthread_mutex_t result_mutex;
    int created = 0;
    int failed = 0;
    double start;
    double end;
    PostSummary summary;

    if (cfg == 0 || out == 0 || metrics == 0 ||
        cfg->trials <= 0 || cfg->time_steps <= 0 || cfg->threads <= 0) {
        return -1;
    }

    metrics_init(metrics);
    metrics->t_total_start = now_sec();
    threads = (pthread_t *)calloc((size_t)cfg->threads, sizeof(*threads));
    args = (ThreadArg *)calloc((size_t)cfg->threads, sizeof(*args));
    if (threads == 0 || args == 0) {
        free(threads);
        free(args);
        return -1;
    }
    if (pthread_mutex_init(&result_mutex, 0) != 0) {
        free(threads);
        free(args);
        return -1;
    }
    result_init(out);

    start = now_sec();
    preprocess_run_extra_work(cfg, (int)((cfg->trials + cfg->batch_size - 1) / cfg->batch_size));
    end = now_sec();
    metrics->t_pre = elapsed_sec(start, end);

    start = now_sec();
    for (int i = 0; i < cfg->threads; ++i) {
        args[i].thread_id = i;
        args[i].cfg = cfg;
        args[i].global_result = out;
        args[i].result_mutex = &result_mutex;
        args[i].metrics = metrics;
        partition_work(cfg->trials, cfg->threads, i, &args[i].start_idx, &args[i].end_idx);
        if (pthread_create(&threads[i], 0, thread_worker, &args[i]) != 0) {
            fprintf(stderr, "pthread_create failed for thread %d\n", i);
            failed = 1;
            break;
        }
        created += 1;
    }
    for (int i = 0; i < created; ++i) {
        if (pthread_join(threads[i], 0) != 0) {
            failed = 1;
        }
    }
    end = now_sec();
    metrics->t_compute = elapsed_sec(start, end);

    start = now_sec();
    if (!failed && cfg->sync_mode == SYNC_REDUCE) {
        result_init(out);
        for (int i = 0; i < cfg->threads; ++i) {
            result_merge(out, &args[i].local_result);
        }
    }
    end = now_sec();
    metrics->t_merge = elapsed_sec(start, end);

    start = now_sec();
    postprocess_finalize(out, cfg->trials, &summary);
    postprocess_run_extra_work(cfg, out);
    end = now_sec();
    metrics->t_post = elapsed_sec(start, end);
    metrics->t_total_end = now_sec();

    pthread_mutex_destroy(&result_mutex);
    free(threads);
    free(args);
    return failed ? -1 : 0;
}

int run_thread_mode(const Config *cfg, Result *out, double *elapsed_out)
{
    StageMetrics metrics;
    int rc = run_thread_mode_metrics(cfg, out, &metrics);
    if (elapsed_out != 0) {
        *elapsed_out = metrics_total(&metrics);
    }
    return rc;
}
