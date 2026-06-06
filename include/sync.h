#ifndef SYNC_H
#define SYNC_H

#include "config.h"

const char *run_mode_to_string(RunMode mode);
const char *sync_mode_to_string(SyncMode mode);
const char *schedule_mode_to_string(ScheduleMode mode);
const char *merge_mode_to_string(MergeMode mode);
const char *ipc_mode_to_string(IpcMode mode);
const char *workload_mode_to_string(WorkloadMode mode);
const char *scaling_mode_to_string(ScalingMode mode);
const char *profile_to_string(WorkloadProfile profile);
const char *affinity_to_string(int enabled);
int parse_run_mode(const char *text, RunMode *out);
int parse_sync_mode(const char *text, SyncMode *out);
int parse_schedule_mode(const char *text, ScheduleMode *out);
int parse_merge_mode(const char *text, MergeMode *out);
int parse_ipc_mode(const char *text, IpcMode *out);
int parse_workload_mode(const char *text, WorkloadMode *out);
int parse_scaling_mode(const char *text, ScalingMode *out);
int parse_profile(const char *text, WorkloadProfile *out);
int parse_on_off(const char *text, int *out);

#endif
