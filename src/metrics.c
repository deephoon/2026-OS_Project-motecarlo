#include "metrics.h"

#include <stddef.h>
#include <sys/resource.h>
#include <time.h>

double now_sec(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

double elapsed_sec(double start, double end)
{
    return end - start;
}

void metrics_init(StageMetrics *m)
{
    if (m == 0) {
        return;
    }
    m->t_total_start = 0.0;
    m->t_total_end = 0.0;
    m->t_pre = 0.0;
    m->t_compute = 0.0;
    m->t_sync = 0.0;
    m->t_ipc = 0.0;
    m->t_merge = 0.0;
    m->t_post = 0.0;
    m->processed_batches = 0;
    m->lock_wait_count = 0;
    m->cond_wait_count = 0;
    m->queue_push_count = 0;
    m->queue_pop_count = 0;
    m->ipc_write_count = 0;
    m->ipc_read_count = 0;
    m->ipc_bytes = 0;
}

double metrics_total(const StageMetrics *m)
{
    if (m == 0) {
        return 0.0;
    }
    return elapsed_sec(m->t_total_start, m->t_total_end);
}

double metrics_compute_ratio(const StageMetrics *m)
{
    double total = metrics_total(m);
    double value;
    if (total <= 0.0) {
        return 0.0;
    }
    value = m->t_compute;
    if (value < 0.0) value = 0.0;
    if (value > total) value = total;
    return value / total;
}

double metrics_sync_ratio(const StageMetrics *m)
{
    double total = metrics_total(m);
    double value;
    if (total <= 0.0) {
        return 0.0;
    }
    value = m->t_sync;
    if (value < 0.0) value = 0.0;
    if (value > total) value = total;
    return value / total;
}

double metrics_merge_ratio(const StageMetrics *m)
{
    double total = metrics_total(m);
    double value;
    if (total <= 0.0) {
        return 0.0;
    }
    value = m->t_merge;
    if (value < 0.0) value = 0.0;
    if (value > total) value = total;
    return value / total;
}

double metrics_ipc_ratio(const StageMetrics *m)
{
    double total = metrics_total(m);
    double value;
    if (total <= 0.0) {
        return 0.0;
    }
    value = m->t_ipc;
    if (value < 0.0) value = 0.0;
    if (value > total) value = total;
    return value / total;
}

double metrics_throughput_batches(const StageMetrics *m)
{
    double total = metrics_total(m);
    return total > 0.0 ? (double)m->processed_batches / total : 0.0;
}

double metrics_sequential_fraction_estimate(const StageMetrics *m)
{
    double total = metrics_total(m);
    if (total <= 0.0) {
        return 0.0;
    }
    {
        double value = m->t_pre + m->t_sync + m->t_ipc +
                       m->t_merge + m->t_post;
        if (value < 0.0) value = 0.0;
        if (value > total) value = total;
        return value / total;
    }
}

int metrics_get_cpu_usage(CpuUsage *usage)
{
    struct rusage ru;
    if (usage == 0 || getrusage(RUSAGE_SELF, &ru) != 0) {
        return -1;
    }
    usage->user_sec = (double)ru.ru_utime.tv_sec +
                      (double)ru.ru_utime.tv_usec / 1000000.0;
    usage->system_sec = (double)ru.ru_stime.tv_sec +
                        (double)ru.ru_stime.tv_usec / 1000000.0;
    return 0;
}
