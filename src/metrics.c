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
    m->t_merge = 0.0;
    m->t_post = 0.0;
    m->processed_batches = 0;
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
    return total > 0.0 ? m->t_compute / total : 0.0;
}

double metrics_sync_ratio(const StageMetrics *m)
{
    double total = metrics_total(m);
    return total > 0.0 ? m->t_sync / total : 0.0;
}

double metrics_merge_ratio(const StageMetrics *m)
{
    double total = metrics_total(m);
    return total > 0.0 ? m->t_merge / total : 0.0;
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
    return (m->t_pre + m->t_sync + m->t_merge + m->t_post) / total;
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
