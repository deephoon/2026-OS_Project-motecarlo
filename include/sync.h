#ifndef SYNC_H
#define SYNC_H

#include "config.h"

const char *run_mode_to_string(RunMode mode);
const char *sync_mode_to_string(SyncMode mode);
const char *schedule_mode_to_string(ScheduleMode mode);
const char *merge_mode_to_string(MergeMode mode);
const char *ipc_mode_to_string(IpcMode mode);
int parse_run_mode(const char *text, RunMode *out);
int parse_sync_mode(const char *text, SyncMode *out);
int parse_schedule_mode(const char *text, ScheduleMode *out);
int parse_merge_mode(const char *text, MergeMode *out);
int parse_ipc_mode(const char *text, IpcMode *out);

#endif
