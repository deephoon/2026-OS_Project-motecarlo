#define _GNU_SOURCE

#include "ideal_benchmark.h"

#include "sync.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include <sched.h>
#endif

typedef struct {
    int worker_id;
    int affinity_enabled;
    int core_count;
    long long iters;
    unsigned long long seed;
    unsigned long long checksum;
} IdealWorkerArg;

static unsigned long long cpu_heavy_kernel(long long iters,
                                           unsigned long long seed)
{
    volatile unsigned long long x = seed;

    for (long long i = 0; i < iters; ++i) {
        x = x * 2862933555777941757ULL + 3037000493ULL;
        x ^= (x >> 17);
        x += (unsigned long long)i;
        x ^= (x << 31);
    }

    return x;
}

static void pin_worker_if_requested(const IdealWorkerArg *arg)
{
    if (arg == 0 || !arg->affinity_enabled) {
        return;
    }
#ifdef __linux__
    {
        int core_count = arg->core_count > 0 ? arg->core_count : 1;
        int core_id = arg->worker_id % core_count;
        cpu_set_t cpuset;
        int rc;

        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);
        rc = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
        if (rc != 0) {
            fprintf(stderr,
                    "warning: pthread_setaffinity_np failed for worker %d core %d\n",
                    arg->worker_id, core_id);
        }
    }
#else
    fprintf(stderr,
            "warning: CPU affinity requested but this platform is not Linux\n");
#endif
}

static void *ideal_worker(void *ptr)
{
    IdealWorkerArg *arg = (IdealWorkerArg *)ptr;
    pin_worker_if_requested(arg);
    arg->checksum = cpu_heavy_kernel(arg->iters, arg->seed);
    return 0;
}

static long long worker_iters_for(const Config *cfg, int worker_id)
{
    if (cfg->scaling_mode == SCALING_UTILIZATION) {
        return cfg->work_iters;
    }
    {
        long long base = cfg->work_iters / cfg->threads;
        long long rem = cfg->work_iters % cfg->threads;
        return base + (worker_id < rem ? 1 : 0);
    }
}

int run_ideal_benchmark(const Config *cfg, IdealResult *out, Metrics *metrics)
{
    pthread_t *threads = 0;
    IdealWorkerArg *args = 0;
    int failed = 0;
    int created_count = 0;
    double start;
    double compute_start;
    double compute_end;
    double merge_start;
    unsigned long long checksum = 1469598103934665603ULL;
    int core_count;

    if (cfg == 0 || out == 0 || metrics == 0 ||
        cfg->threads <= 0 || cfg->work_iters <= 0) {
        return -1;
    }

    metrics_init(metrics);
    start = now_sec();
    metrics->t_total_start = start;
    compute_start = start;

    threads = (pthread_t *)calloc((size_t)cfg->threads, sizeof(*threads));
    args = (IdealWorkerArg *)calloc((size_t)cfg->threads, sizeof(*args));
    if (threads == 0 || args == 0) {
        free(threads);
        free(args);
        return -1;
    }

    core_count = cfg->core_count > 0 ? cfg->core_count : cfg->threads;
    for (int i = 0; i < cfg->threads; ++i) {
        args[i].worker_id = i;
        args[i].affinity_enabled = cfg->affinity_enabled;
        args[i].core_count = core_count;
        args[i].iters = worker_iters_for(cfg, i);
        args[i].seed = ((unsigned long long)cfg->seed << 32) ^
                       (unsigned long long)(i + 1) * 0x9e3779b97f4a7c15ULL ^
                       (unsigned long long)args[i].iters;
        if (pthread_create(&threads[i], 0, ideal_worker, &args[i]) != 0) {
            failed = 1;
            break;
        }
        created_count++;
    }

    for (int i = 0; i < created_count; ++i) {
        if (pthread_join(threads[i], 0) != 0) {
            failed = 1;
        }
    }
    compute_end = now_sec();

    merge_start = now_sec();
    for (int i = 0; i < cfg->threads; ++i) {
        checksum ^= args[i].checksum + 0x9e3779b97f4a7c15ULL +
                    (checksum << 6) + (checksum >> 2);
    }
    metrics->t_merge = elapsed_sec(merge_start, now_sec());
    metrics->t_compute = elapsed_sec(compute_start, compute_end);
    metrics->t_total_end = now_sec();
    out->time_total = metrics_total(metrics);
    out->checksum = checksum;

    free(threads);
    free(args);
    return failed ? -1 : 0;
}
