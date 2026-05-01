#ifndef SYNC_H
#define SYNC_H

#include "config.h"

/* Converts mode enum values to stable CSV strings. */
const char *run_mode_to_string(RunMode mode);

/* Converts sync enum values to stable CSV strings. */
const char *sync_mode_to_string(SyncMode mode);

/* Parses CLI mode names. Returns 0 on success. */
int parse_run_mode(const char *text, RunMode *out);

/* Parses CLI sync names. Returns 0 on success. */
int parse_sync_mode(const char *text, SyncMode *out);

#endif
