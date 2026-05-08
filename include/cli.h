#ifndef CLI_H
#define CLI_H

#include "config.h"

#include <stdio.h>

void cli_print_usage(FILE *out, const char *program);
int cli_parse_args(int argc, char **argv, Config *cfg);

#endif
