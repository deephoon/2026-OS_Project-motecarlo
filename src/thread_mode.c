#include "thread_mode.h"

#include "metrics.h"
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
} ThreadArg;

static void record_trial_result(ThreadArg *arg, RiskLevel risk, int collided)
{
    switch (arg->cfg->sync_mode) {
    case SYNC_REDUCE:
        /* Local reduce avoids shared writes in the hot loop. Each thread
         * writes only to its own cache-local Result, then the main thread
         * merges after pthread_join. */
        result_add_trial(&arg->local_result, risk, collided);
        break;
    case SYNC_MUTEX:
        /* Mutex mode is correct but intentionally high-overhead: every
         * trial serializes the update to the shared aggregate. */
        pthread_mutex_lock(arg->result_mutex);
        result_add_trial(arg->global_result, risk, collided);
        pthread_mutex_unlock(arg->result_mutex);
        break;
    case SYNC_NOSYNC:
        /* NOSYNC intentionally demonstrates a race condition. Multiple
         * threads update the same counters without mutual exclusion, so
         * histogram sums and totals may be lower than requested trials. */
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
        unsigned int trial_seed = simulation_seed_for_trial(cfg->seed, i);
        int collided = 0;
        RiskLevel risk = run_trial(&trial_seed, cfg->time_steps, &collided);

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

static void thread_arg_init(ThreadArg *arg, int thread_id, const Config *cfg,
                            Result *global_result,
                            pthread_mutex_t *result_mutex)
{
    arg->thread_id = thread_id;
    arg->cfg = cfg;
    arg->global_result = global_result;
    arg->result_mutex = result_mutex;
    result_init(&arg->local_result);
    partition_work(cfg->trials, cfg->threads, thread_id,
                   &arg->start_idx, &arg->end_idx);
}

static int allocate_thread_state(const Config *cfg, pthread_t **threads_out,
                                 ThreadArg **args_out)
{
    *threads_out = (pthread_t *)calloc((size_t)cfg->threads,
                                       sizeof(**threads_out));
    *args_out = (ThreadArg *)calloc((size_t)cfg->threads, sizeof(**args_out));
    if (*threads_out == 0 || *args_out == 0) {
        free(*threads_out);
        free(*args_out);
        *threads_out = 0;
        *args_out = 0;
        return -1;
    }
    return 0;
}

static int create_workers(const Config *cfg, pthread_t *threads,
                          ThreadArg *args, Result *out,
                          pthread_mutex_t *result_mutex)
{
    int created = 0;

    for (int i = 0; i < cfg->threads; ++i) {
        int rc;

        thread_arg_init(&args[i], i, cfg, out, result_mutex);

        /* pthread_create is the OS-facing primitive under test: it creates
         * independent kernel-scheduled execution contexts for CPU-bound work. */
        rc = pthread_create(&threads[i], 0, thread_worker, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed for thread %d: %d\n", i, rc);
            break;
        }
        created += 1;
    }

    return created;
}

static int join_workers(pthread_t *threads, int created)
{
    int failed = 0;

    for (int i = 0; i < created; ++i) {
        int rc = pthread_join(threads[i], 0);
        if (rc != 0) {
            fprintf(stderr, "pthread_join failed for thread %d: %d\n", i, rc);
            failed = 1;
        }
    }
    return failed ? -1 : 0;
}

static void merge_local_results(Result *out, const ThreadArg *args, int count)
{
    result_init(out);
    for (int i = 0; i < count; ++i) {
        result_merge(out, &args[i].local_result);
    }
}

int run_thread_mode(const Config *cfg, Result *out, double *elapsed_out)
{
    pthread_t *threads = 0;
    ThreadArg *args = 0;
    pthread_mutex_t result_mutex;
    int mutex_ready = 0;
    double start;
    double end;
    int created = 0;
    int failed = 0;

    if (cfg == 0 || out == 0 || elapsed_out == 0 ||
        cfg->trials <= 0 || cfg->time_steps <= 0 || cfg->threads <= 0) {
        return -1;
    }

    if (allocate_thread_state(cfg, &threads, &args) != 0) {
        return -1;
    }

    result_init(out);
    if (pthread_mutex_init(&result_mutex, 0) != 0) {
        free(threads);
        free(args);
        return -1;
    }
    mutex_ready = 1;

    start = now_sec();
    created = create_workers(cfg, threads, args, out, &result_mutex);
    if (created != cfg->threads) {
        failed = 1;
    }
    if (join_workers(threads, created) != 0) {
        failed = 1;
    }

    if (!failed && cfg->sync_mode == SYNC_REDUCE) {
        merge_local_results(out, args, cfg->threads);
    }

    end = now_sec();
    *elapsed_out = elapsed_sec(start, end);
    out->checksum = result_compute_checksum(out);

    if (mutex_ready) {
        pthread_mutex_destroy(&result_mutex);
    }
    free(threads);
    free(args);
    return failed ? -1 : 0;
}
