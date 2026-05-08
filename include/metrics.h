#ifndef METRICS_H
#define METRICS_H

typedef struct {
    double user_sec;
    double system_sec;
} CpuUsage;

typedef struct {
    double t_total_start;
    double t_total_end;
    double t_pre;
    double t_compute;
    double t_sync;
    double t_merge;
    double t_post;
    long processed_batches;
} StageMetrics;

double now_sec(void);
double elapsed_sec(double start, double end);
void metrics_init(StageMetrics *m);
double metrics_total(const StageMetrics *m);
double metrics_compute_ratio(const StageMetrics *m);
double metrics_sync_ratio(const StageMetrics *m);
double metrics_merge_ratio(const StageMetrics *m);
double metrics_throughput_batches(const StageMetrics *m);
double metrics_sequential_fraction_estimate(const StageMetrics *m);
int metrics_get_cpu_usage(CpuUsage *usage);

#endif
