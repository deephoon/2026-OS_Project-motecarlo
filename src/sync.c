#include "sync.h"

#include <string.h>

const char *run_mode_to_string(RunMode mode)
{
    switch (mode) {
    case MODE_SEQ: return "seq";
    case MODE_THREAD: return "thread";
    case MODE_PIPELINE: return "pipeline";
    case MODE_PROCESS: return "process";
    case MODE_HYBRID: return "hybrid";
    default: return "unknown";
    }
}

const char *sync_mode_to_string(SyncMode mode)
{
    switch (mode) {
    case SYNC_NOSYNC: return "nosync";
    case SYNC_MUTEX: return "mutex";
    case SYNC_REDUCE: return "reduce";
    default: return "unknown";
    }
}

const char *schedule_mode_to_string(ScheduleMode mode)
{
    switch (mode) {
    case SCHEDULE_STATIC: return "static";
    case SCHEDULE_QUEUE: return "queue";
    default: return "unknown";
    }
}

const char *merge_mode_to_string(MergeMode mode)
{
    switch (mode) {
    case MERGE_FINAL: return "final";
    case MERGE_INTERACTIVE: return "interactive";
    default: return "unknown";
    }
}

const char *ipc_mode_to_string(IpcMode mode)
{
    switch (mode) {
    case IPC_PIPE: return "pipe";
    case IPC_SHM: return "shm";
    default: return "unknown";
    }
}

const char *workload_mode_to_string(WorkloadMode mode)
{
    switch (mode) {
    case WORKLOAD_UNIFORM: return "uniform";
    case WORKLOAD_SKEWED: return "skewed";
    default: return "unknown";
    }
}

int parse_run_mode(const char *text, RunMode *out)
{
    if (text == 0 || out == 0) return -1;
    if (strcmp(text, "seq") == 0) *out = MODE_SEQ;
    else if (strcmp(text, "thread") == 0) *out = MODE_THREAD;
    else if (strcmp(text, "pipeline") == 0) *out = MODE_PIPELINE;
    else if (strcmp(text, "process") == 0) *out = MODE_PROCESS;
    else if (strcmp(text, "hybrid") == 0) *out = MODE_HYBRID;
    else return -1;
    return 0;
}

int parse_sync_mode(const char *text, SyncMode *out)
{
    if (text == 0 || out == 0) return -1;
    if (strcmp(text, "nosync") == 0) *out = SYNC_NOSYNC;
    else if (strcmp(text, "mutex") == 0) *out = SYNC_MUTEX;
    else if (strcmp(text, "reduce") == 0) *out = SYNC_REDUCE;
    else return -1;
    return 0;
}

int parse_schedule_mode(const char *text, ScheduleMode *out)
{
    if (text == 0 || out == 0) return -1;
    if (strcmp(text, "static") == 0) *out = SCHEDULE_STATIC;
    else if (strcmp(text, "queue") == 0) *out = SCHEDULE_QUEUE;
    else return -1;
    return 0;
}

int parse_merge_mode(const char *text, MergeMode *out)
{
    if (text == 0 || out == 0) return -1;
    if (strcmp(text, "final") == 0) *out = MERGE_FINAL;
    else if (strcmp(text, "interactive") == 0) *out = MERGE_INTERACTIVE;
    else return -1;
    return 0;
}

int parse_ipc_mode(const char *text, IpcMode *out)
{
    if (text == 0 || out == 0) return -1;
    if (strcmp(text, "pipe") == 0) *out = IPC_PIPE;
    else if (strcmp(text, "shm") == 0) *out = IPC_SHM;
    else return -1;
    return 0;
}

int parse_workload_mode(const char *text, WorkloadMode *out)
{
    if (text == 0 || out == 0) return -1;
    if (strcmp(text, "uniform") == 0) *out = WORKLOAD_UNIFORM;
    else if (strcmp(text, "skewed") == 0) *out = WORKLOAD_SKEWED;
    else return -1;
    return 0;
}
