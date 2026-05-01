#ifndef METRICS_H
#define METRICS_H

typedef struct {
    double user_sec;
    double system_sec;
} CpuUsage;

/* Returns a monotonic wall-clock timestamp in seconds. */
double now_sec(void);

/* Returns the elapsed seconds between two timestamps. */
double elapsed_sec(double start, double end);

/* Optional CPU usage hook for final-project analysis. */
int metrics_get_cpu_usage(CpuUsage *usage);

#endif
