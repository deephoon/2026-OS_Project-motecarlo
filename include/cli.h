#ifndef CLI_H
#define CLI_H

#include "config.h"

#include <stdio.h>

/* Prints command-line usage text. */
void cli_print_usage(FILE *out, const char *program);

/* Parses command-line arguments into Config. Returns 0 on success. */
int cli_parse_args(int argc, char **argv, Config *cfg);

#endif
