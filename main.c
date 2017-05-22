/*
 * main.c
 * Copyright (C) 2017 Kovid Goyal <kovid at kovidgoyal.net>
 *
 * Distributed under terms of the GPL3 license.
 */

#include "data-types.h"
#include "cli.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static int 
read_stdin() {
    char *linebuf = NULL;
    size_t n = 0;
    ssize_t sz = 0;
    int ret = 0;

    while (true) {
        sz = getline(&linebuf, &n, stdin);
        if (sz < 1) {
            if (!feof(stdin)) {
                perror("Failed to read from STDIN with error:"); 
                ret = 1;
            }
            break;
        }
        printf("%s", linebuf);
    }

    if (linebuf) free(linebuf);
    linebuf = NULL;
    return ret;
}


int 
main(int argc, char *argv[]) {
    struct gengetopt_args_info opts;
    int ret = 0;
    char *needle = NULL;

    if (cmdline_parser(argc, argv, &opts) != 0) return 1;
    if (opts.inputs_num != 1) {
        fprintf(stderr, "You must specify a single query\n");
        ret = 1; 
        goto end;
    }
    needle = opts.inputs[0];
    printf("limit: %d needle: %s\n", opts.limit_arg, needle);
    ret = read_stdin();

end:
    cmdline_parser_free(&opts);
    return ret;
}

