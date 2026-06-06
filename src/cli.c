#include "cli.h"

#include "sync.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

void cli_print_usage(FILE *out, const char *program)
{
    fprintf(out,
            "Usage: %s [options]\n"
            "  --mode <seq|thread|pipeline|process|hybrid>\n"
            "  --schedule <static|queue>\n"
            "  --merge <final|interactive>\n"
            "  --trials <int> --steps <int> --threads <int>\n"
            "  --processes <int> --batch-size <int> --queue-size <int>\n"
            "  --workload <uniform|skewed> --skew-factor <int>\n"
            "  --profile <default|process_friendly|thread_friendly>\n"
            "  --inner-work <int>\n"
            "  --affinity <on|off> --core-count <int>\n"
            "  --pre-work <int> --post-work <int>\n"
            "  --sync <nosync|mutex|reduce> --ipc <pipe|shm>\n"
            "  --enable-pipeline <0|1> --metrics-detail <0|1>\n"
            "  --seed <int> --verbose --help\n",
            program);
}

static int parse_long_arg(const char *text, long min_value, long max_value,
                          long *out)
{
    char *end = 0;
    long value;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        value < min_value || value > max_value) {
        return -1;
    }
    *out = value;
    return 0;
}

static int parse_ll_arg(const char *text, long long min_value,
                        long long max_value, long long *out)
{
    char *end = 0;
    long long value;
    errno = 0;
    value = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        value < min_value || value > max_value) {
        return -1;
    }
    *out = value;
    return 0;
}

static int require_value(int argc, int *index)
{
    *index += 1;
    return *index < argc ? 0 : -1;
}

static int parse_int_option(int argc, char **argv, int *index,
                            long min_value, long max_value, int *out)
{
    long value;
    if (require_value(argc, index) != 0 ||
        parse_long_arg(argv[*index], min_value, max_value, &value) != 0) {
        return -1;
    }
    *out = (int)value;
    return 0;
}

int cli_parse_args(int argc, char **argv, Config *cfg)
{
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0) {
            cli_print_usage(stdout, argv[0]);
            exit(0);
        } else if (strcmp(arg, "--verbose") == 0) {
            cfg->verbose = 1;
        } else if (strcmp(arg, "--mode") == 0) {
            if (require_value(argc, &i) != 0 || parse_run_mode(argv[i], &cfg->mode) != 0) return -1;
        } else if (strcmp(arg, "--schedule") == 0) {
            if (require_value(argc, &i) != 0 || parse_schedule_mode(argv[i], &cfg->schedule_mode) != 0) return -1;
        } else if (strcmp(arg, "--merge") == 0) {
            if (require_value(argc, &i) != 0 || parse_merge_mode(argv[i], &cfg->merge_mode) != 0) return -1;
        } else if (strcmp(arg, "--sync") == 0) {
            if (require_value(argc, &i) != 0 || parse_sync_mode(argv[i], &cfg->sync_mode) != 0) return -1;
        } else if (strcmp(arg, "--ipc") == 0) {
            if (require_value(argc, &i) != 0 || parse_ipc_mode(argv[i], &cfg->ipc_mode) != 0) return -1;
        } else if (strcmp(arg, "--workload") == 0) {
            if (require_value(argc, &i) != 0 || parse_workload_mode(argv[i], &cfg->workload_mode) != 0) return -1;
        } else if (strcmp(arg, "--profile") == 0) {
            if (require_value(argc, &i) != 0 || parse_profile(argv[i], &cfg->profile) != 0) return -1;
        } else if (strcmp(arg, "--trials") == 0) {
            if (require_value(argc, &i) != 0 || parse_long_arg(argv[i], 1, LONG_MAX, &cfg->trials) != 0) return -1;
        } else if (strcmp(arg, "--steps") == 0) {
            if (parse_int_option(argc, argv, &i, 1, INT_MAX, &cfg->time_steps) != 0) return -1;
        } else if (strcmp(arg, "--threads") == 0) {
            if (parse_int_option(argc, argv, &i, 1, INT_MAX, &cfg->threads) != 0) return -1;
        } else if (strcmp(arg, "--processes") == 0) {
            if (parse_int_option(argc, argv, &i, 1, INT_MAX, &cfg->processes) != 0) return -1;
        } else if (strcmp(arg, "--batch-size") == 0) {
            if (parse_int_option(argc, argv, &i, 1, INT_MAX, &cfg->batch_size) != 0) return -1;
        } else if (strcmp(arg, "--queue-size") == 0) {
            if (parse_int_option(argc, argv, &i, 1, INT_MAX, &cfg->queue_size) != 0) return -1;
        } else if (strcmp(arg, "--skew-factor") == 0) {
            if (parse_int_option(argc, argv, &i, 1, INT_MAX, &cfg->skew_factor) != 0) return -1;
        } else if (strcmp(arg, "--inner-work") == 0) {
            if (require_value(argc, &i) != 0 ||
                parse_ll_arg(argv[i], 0, LLONG_MAX, &cfg->inner_work) != 0) return -1;
        } else if (strcmp(arg, "--affinity") == 0) {
            if (require_value(argc, &i) != 0 || parse_on_off(argv[i], &cfg->affinity_enabled) != 0) return -1;
        } else if (strcmp(arg, "--core-count") == 0) {
            if (parse_int_option(argc, argv, &i, 0, INT_MAX, &cfg->core_count) != 0) return -1;
        } else if (strcmp(arg, "--pre-work") == 0) {
            if (parse_int_option(argc, argv, &i, 0, INT_MAX, &cfg->pre_work) != 0) return -1;
        } else if (strcmp(arg, "--post-work") == 0) {
            if (parse_int_option(argc, argv, &i, 0, INT_MAX, &cfg->post_work) != 0) return -1;
        } else if (strcmp(arg, "--enable-pipeline") == 0) {
            if (parse_int_option(argc, argv, &i, 0, 1, &cfg->enable_pipeline) != 0) return -1;
        } else if (strcmp(arg, "--metrics-detail") == 0) {
            if (parse_int_option(argc, argv, &i, 0, 1, &cfg->metrics_detail) != 0) return -1;
        } else if (strcmp(arg, "--seed") == 0) {
            long value;
            if (require_value(argc, &i) != 0 ||
                parse_long_arg(argv[i], 0, UINT_MAX, &value) != 0) return -1;
            cfg->seed = (unsigned int)value;
        } else {
            return -1;
        }
    }
    return config_validate(cfg) ? 0 : -1;
}
