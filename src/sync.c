#include "sync.h"

#include <string.h>

const char *run_mode_to_string(RunMode mode)
{
    switch (mode) {
    case MODE_SEQ:
        return "seq";
    case MODE_THREAD:
        return "thread";
    default:
        return "unknown";
    }
}

const char *sync_mode_to_string(SyncMode mode)
{
    switch (mode) {
    case SYNC_NOSYNC:
        return "nosync";
    case SYNC_MUTEX:
        return "mutex";
    case SYNC_REDUCE:
        return "reduce";
    default:
        return "unknown";
    }
}

int parse_run_mode(const char *text, RunMode *out)
{
    if (text == 0 || out == 0) {
        return -1;
    }
    if (strcmp(text, "seq") == 0) {
        *out = MODE_SEQ;
        return 0;
    }
    if (strcmp(text, "thread") == 0) {
        *out = MODE_THREAD;
        return 0;
    }
    return -1;
}

int parse_sync_mode(const char *text, SyncMode *out)
{
    if (text == 0 || out == 0) {
        return -1;
    }
    if (strcmp(text, "nosync") == 0) {
        *out = SYNC_NOSYNC;
        return 0;
    }
    if (strcmp(text, "mutex") == 0) {
        *out = SYNC_MUTEX;
        return 0;
    }
    if (strcmp(text, "reduce") == 0) {
        *out = SYNC_REDUCE;
        return 0;
    }
    return -1;
}
