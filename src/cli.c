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
            "\n"
            "Options:\n"
            "  --mode <seq|thread>          Run mode (default: seq)\n"
            "  --trials <int>               Number of Monte Carlo trials (default: 100000)\n"
            "  --steps <int>                Time steps per trial (default: 50)\n"
            "  --threads <int>              Thread count for thread mode (default: 4)\n"
            "  --sync <nosync|mutex|reduce> Synchronization mode (default: reduce)\n"
            "  --seed <int>                 Deterministic base seed (default: 42)\n"
            "  --verbose                    Print human-readable summary to stderr\n"
            "  --help                       Show this help text\n",
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

static int require_value(int argc, int *index)
{
    *index += 1;
    return *index < argc ? 0 : -1;
}

static int parse_positive_int_option(int argc, char **argv, int *index,
                                     int *out)
{
    long value;

    if (require_value(argc, index) != 0 ||
        parse_long_arg(argv[*index], 1, INT_MAX, &value) != 0) {
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
        }
        if (strcmp(arg, "--verbose") == 0) {
            cfg->verbose = 1;
        } else if (strcmp(arg, "--mode") == 0) {
            if (require_value(argc, &i) != 0 ||
                parse_run_mode(argv[i], &cfg->mode) != 0) {
                return -1;
            }
        } else if (strcmp(arg, "--sync") == 0) {
            if (require_value(argc, &i) != 0 ||
                parse_sync_mode(argv[i], &cfg->sync_mode) != 0) {
                return -1;
            }
        } else if (strcmp(arg, "--trials") == 0) {
            if (require_value(argc, &i) != 0 ||
                parse_long_arg(argv[i], 1, LONG_MAX, &cfg->trials) != 0) {
                return -1;
            }
        } else if (strcmp(arg, "--steps") == 0) {
            if (parse_positive_int_option(argc, argv, &i,
                                          &cfg->time_steps) != 0) {
                return -1;
            }
        } else if (strcmp(arg, "--threads") == 0) {
            if (parse_positive_int_option(argc, argv, &i,
                                          &cfg->threads) != 0) {
                return -1;
            }
        } else if (strcmp(arg, "--seed") == 0) {
            long value;
            if (require_value(argc, &i) != 0 ||
                parse_long_arg(argv[i], 0, UINT_MAX, &value) != 0) {
                return -1;
            }
            cfg->seed = (unsigned int)value;
        } else {
            return -1;
        }
    }

    return config_validate(cfg) ? 0 : -1;
}
