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
read_stdin(char *needle, char* level1, char* level2, char *level3) {
    char *linebuf = NULL;
    size_t n = 0;
    ssize_t sz = 0;
    int ret = 0;

    while (true) {
        errno = 0;
        sz = getline(&linebuf, &n, stdin);
        if (sz < 1) {
            if (errno != 0) {
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
    ret = read_stdin(needle, opts.level1_arg, opts.level2_arg, opts.level3_arg);

end:
    cmdline_parser_free(&opts);
    return ret;
}

