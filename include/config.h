#ifndef CONFIG_H
#define CONFIG_H

typedef enum {
    MODE_SEQ = 0,
    MODE_THREAD = 1
} RunMode;

typedef enum {
    SYNC_NOSYNC = 0,
    SYNC_MUTEX = 1,
    SYNC_REDUCE = 2
} SyncMode;

typedef struct {
    RunMode mode;
    SyncMode sync_mode;
    long trials;
    int time_steps;
    int threads;
    unsigned int seed;
    int verbose;
} Config;

/* Fills a Config with the documented CLI defaults. */
void config_set_defaults(Config *cfg);

/* Validates numeric and enum ranges before dispatching a run. */
int config_validate(const Config *cfg);

#endif
