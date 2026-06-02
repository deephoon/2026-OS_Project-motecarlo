#ifndef CONFIG_H
#define CONFIG_H

typedef enum {
    MODE_SEQ = 0,
    MODE_THREAD = 1,
    MODE_PIPELINE = 2,
    MODE_PROCESS = 3,
    MODE_HYBRID = 4,
    MODE_IDEAL = 5
} RunMode;

typedef enum {
    SYNC_NOSYNC = 0,
    SYNC_MUTEX = 1,
    SYNC_REDUCE = 2
} SyncMode;

typedef enum {
    SCHEDULE_STATIC = 0,
    SCHEDULE_QUEUE = 1
} ScheduleMode;

typedef enum {
    MERGE_FINAL = 0,
    MERGE_INTERACTIVE = 1
} MergeMode;

typedef enum {
    IPC_PIPE = 0,
    IPC_SHM = 1
} IpcMode;

typedef enum {
    WORKLOAD_UNIFORM = 0,
    WORKLOAD_SKEWED = 1
} WorkloadMode;

typedef enum {
    SCALING_STRONG = 0,
    SCALING_UTILIZATION = 1
} ScalingMode;

typedef struct {
    RunMode mode;
    SyncMode sync_mode;
    ScheduleMode schedule_mode;
    MergeMode merge_mode;
    IpcMode ipc_mode;
    WorkloadMode workload_mode;
    ScalingMode scaling_mode;
    long trials;
    long long work_iters;
    int time_steps;
    int threads;
    int processes;
    int batch_size;
    int queue_size;
    int skew_factor;
    int affinity_enabled;
    int core_count;
    int pre_work;
    int post_work;
    int enable_pipeline;
    int metrics_detail;
    unsigned int seed;
    int verbose;
} Config;

void config_set_defaults(Config *cfg);
int config_validate(const Config *cfg);

#endif
