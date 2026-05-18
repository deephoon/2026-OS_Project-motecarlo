#ifndef CONFIG_H
#define CONFIG_H

typedef enum {
    MODE_SEQ = 0,
    MODE_THREAD = 1,
    MODE_PIPELINE = 2,
    MODE_PROCESS = 3,
    MODE_HYBRID = 4
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

typedef struct {
    RunMode mode;
    SyncMode sync_mode;
    ScheduleMode schedule_mode;
    MergeMode merge_mode;
    IpcMode ipc_mode;
    long trials;
    int time_steps;
    int threads;
    int processes;
    int batch_size;
    int queue_size;
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
