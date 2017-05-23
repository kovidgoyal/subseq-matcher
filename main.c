/*
 * main.c
 * Copyright (C) 2017 Kovid Goyal <kovid at kovidgoyal.net>
 *
 * Distributed under terms of the GPL3 license.
 */

#include "data-types.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

static void 
run_scoring(void *workspace, Candidate *haystack, size_t start, size_t count, char *needle, len_t needle_len, char *level1, char *level2, char* level3) {
    for (size_t i = start; i < count; i++) {
        haystack[i].score = score_item(workspace, haystack[i].src, haystack[i].haystack_len, haystack[i].positions);
    }

}


static int 
read_stdin(char *needle, args_info *opts) {
    char *linebuf = NULL;
    size_t n = 0, idx = 0;
    len_t needle_len = strlen(needle);
    ssize_t sz = 0;
    int ret = 0;
    Candidates candidates = {0};
    Chars chars = {0};

    if (needle_len < 1) { fprintf(stderr, "Empty query not allowed.\n"); return 1; }
    ALLOC_VEC(char, chars, 8192 * 20);
    ALLOC_VEC(Candidate, candidates, 8192);
    if (chars.data == NULL || candidates.data == NULL) return 1;

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
                ENSURE_SPACE(char, chars, sz);
                ENSURE_SPACE(Candidate, candidates, 1);
                memcpy(&NEXT(chars), linebuf, sz);
                NEXT(candidates).src_sz = sz;
                NEXT(candidates).haystack_len = MIN(LEN_MAX, sz - 1);
                NEXT(candidates).idx = idx++;
                INC(candidates, 1); INC(chars, sz); 
            }
        }
    }

    Candidate *haystack = &ITEM(candidates, 0);
    len_t *positions = (len_t*)calloc(SIZE(candidates), sizeof(len_t) * needle_len);
    len_t max_haystack_len = 0;
    for (size_t i = 0; i < SIZE(candidates); i++) max_haystack_len = MAX(max_haystack_len, haystack[i].haystack_len);
    void *w = alloc_workspace(max_haystack_len, needle_len, needle, opts->level1_arg, opts->level2_arg, opts->level3_arg);
    if (positions && w) {
        char *cdata = &ITEM(chars, 0);
        for (size_t i = 0, off = 0; i < SIZE(candidates); i++) {
            haystack[i].positions = positions + (i * needle_len);
            haystack[i].src = cdata + off;
            off += haystack[i].src_sz;
        }
        run_scoring(w, haystack, 0, SIZE(candidates), needle, needle_len, opts->level1_arg, opts->level2_arg, opts->level3_arg);
        output_results(haystack, SIZE(candidates), opts, needle_len);
    } else {
        ret = 1; REPORT_OOM;
    }

    if (linebuf) free(linebuf);
    linebuf = NULL;
    w = free_workspace(w);
    FREE_VEC(chars); free(positions); FREE_VEC(candidates);
    return ret;
}


int 
main(int argc, char *argv[]) {
    args_info opts;
    int ret = 0;
    char *needle = NULL;

    if (cmdline_parser(argc, argv, &opts) != 0) return 1;
    if (opts.inputs_num != 1) {
        fprintf(stderr, "You must specify a single query\n");
        ret = 1; 
        goto end;
    }
    needle = opts.inputs[0];
    if (strlen(needle) > LEN_MAX) {
        fprintf(stderr, "The query must be no longer than %d bytes\n", LEN_MAX);
        ret = 1;
        goto end;
    }
    ret = read_stdin(needle, &opts);

end:
    cmdline_parser_free(&opts);
    return ret;
}

