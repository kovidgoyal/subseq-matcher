/*
 * main.c
 * Copyright (C) 2017 Kovid Goyal <kovid at kovidgoyal.net>
 *
 * Distributed under terms of the GPL3 license.
 */

#include "data-types.h"
#include "vector.h"
#include "cli.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


static int
run_scoring(Candidate *haystack, char *needle, int32_t needle_len, int32_t max_haystack_len) {
    int ret = 0;
    CacheItem ***cache = alloc_cache(needle_len, max_haystack_len);
    Stack *stack = alloc_stack(needle_len, max_haystack_len);
    if (cache == NULL || stack == NULL) { REPORT_OOM; free(stack); free(cache); return 1; }


    stack = free_stack(stack);
    cache = free_cache(cache);
    return ret;
}


static int 
read_stdin(char *needle, char* level1, char* level2, char *level3) {
    char *linebuf = NULL;
    size_t n = 0, needle_len = strlen(needle), max_haystack_len = 0;
    ssize_t sz = 0;
    int ret = 0;
    Positions positions = {0};
    Candidates candidates = {0};
    Chars chars = {0};

    if (needle_len < 1) { fprintf(stderr, "Empty query not allowed.\n"); return 1; }
    ALLOC_VEC(char, chars, 8192 * 20);
    ALLOC_VEC(int32_t, positions, 8192 * needle_len * 2);
    ALLOC_VEC(Candidate, candidates, 8192);
    if (chars.data == NULL || positions.data == NULL || candidates.data == NULL) return 1;

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
        if (sz > 1) {
            if (linebuf[sz - 1] == '\n') linebuf[--sz] = 0;
            sz++;  // sz does not include the trailing null byte
            if (sz > 0) {
                max_haystack_len = MAX(max_haystack_len, sz);
                ENSURE_SPACE(char, chars, sz);
                ENSURE_SPACE(int32_t, positions, 2*needle_len);
                ENSURE_SPACE(Candidate, candidates, 1);
                memcpy(&NEXT(chars), linebuf, sz);
                NEXT(candidates).src = &NEXT(chars);
                NEXT(candidates).src_sz = sz - 1;
                NEXT(candidates).positions = &NEXT(positions);
                NEXT(candidates).needlebuf = (&NEXT(positions)) + needle_len;
                INC(candidates, 1); INC(chars, sz); INC(positions, 2*needle_len);
            }
        }
    }

    Candidate *haystack = &ITEM(candidates, 0);

    ret = run_scoring(haystack, needle, needle_len, max_haystack_len);

    if (linebuf) free(linebuf);
    linebuf = NULL;
    FREE_VEC(chars); FREE_VEC(positions); FREE_VEC(candidates);
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

