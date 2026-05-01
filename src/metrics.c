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

int metrics_get_cpu_usage(CpuUsage *usage)
{
    struct rusage ru;

    if (usage == NULL) {
        return -1;
    }
    if (getrusage(RUSAGE_SELF, &ru) != 0) {
        return -1;
    }

    usage->user_sec = (double)ru.ru_utime.tv_sec +
                      (double)ru.ru_utime.tv_usec / 1000000.0;
    usage->system_sec = (double)ru.ru_stime.tv_sec +
                        (double)ru.ru_stime.tv_usec / 1000000.0;
    return 0;
}
