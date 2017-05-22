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
#include <ctype.h>

typedef struct gengetopt_args_info args_info;
static char mark_before[100] = {0}, mark_after[100] = {0};

static void
unescape(char *src, char *dest, size_t destlen) {
    size_t srclen = strlen(src);
    char buf[5] = {0};

    for (size_t i = 0, j = 0; i < MIN(srclen, destlen); i++, j++) {
        if (src[i] == '\\' && i < srclen - 1) {
            switch(src[++i]) {
#define S(x) dest[j] = x; break;
                case 'e':
                case 'E':
                    S(0x1b);
                case 'x':
                    if (i + 1 < srclen && isxdigit(src[i]) && isxdigit(src[i+1])) {
                        buf[0] = src[i]; buf[1] = src[i+1]; buf[2] = 0;
                        dest[j] = (unsigned char)(strtol(buf, NULL, 16));
                        i += 2;
                        break;
                    } 
                default:
                    S(src[i+1]);
            }
#undef S
        } else dest[j] = src[i];
    }
}


#define FIELD(x, which) (((Candidate*)(x))->which)

static int 
cmpscore(const void *a, const void *b) {
    double sa = FIELD(a, score), sb = FIELD(b, score);
    // Sort descending
    return (sa > sb) ? -1 : ((sa == sb) ? (FIELD(a, idx) - FIELD(b, idx)) : 1);
}

static void
output_with_marks(char *src, int32_t *positions, int32_t poslen) {
    int i, pos, j = 0;
    size_t l = strlen(src);
    for (pos = 0; pos < poslen; pos++, j++) {
        for (i = j; i < MIN(l, positions[pos]); i++) printf("%c", src[i]);
        j = positions[pos];
        if (j < l) printf("%s%c%s", mark_before, src[j], mark_after);
    }
    for (i = positions[poslen-1] + 1; i < l; i++) printf("%c", src[i]);
    printf("\n");
}


static void
output_result(Candidate *c, args_info *opts, int32_t needle_len) {
    if (c->score > 0.0 && (mark_before[0] || mark_after[0])) {
        output_with_marks(c->src, c->positions, needle_len);
    } else printf("%s\n", c->src);
}


static void
output_results(Candidate *haystack, size_t count, args_info *opts, int32_t needle_len) {
    qsort(haystack, count, sizeof(*haystack), cmpscore);
    size_t left = opts->limit_arg > 0 ? opts->limit_arg : count;
    for (size_t i = 0; i < left; i++) {
        output_result(haystack + i, opts, needle_len);
    }
}


static int
run_scoring(Candidate *haystack, size_t start, size_t count, char *needle, int32_t needle_len, int32_t max_haystack_len, char *level1, char *level2, char* level3) {
    int ret = 0;
    CacheItem ***cache = alloc_cache(needle_len, max_haystack_len);
    Stack *stack = alloc_stack(needle_len, max_haystack_len);
    int32_t *posbuf = (int32_t*)calloc(needle_len, sizeof(int32_t));
    if (cache == NULL || stack == NULL || posbuf == NULL) { REPORT_OOM; free(stack); free(cache); free(posbuf); return 1; }

    MatchInfo mi = {0};

    for (size_t i = start; i < count; i++) {
        mi.haystack = haystack[i].src;
        mi.haystack_len = haystack[i].src_sz;
        mi.needle = needle;
        mi.needle_len = needle_len;
        mi.max_score_per_char = (1.0 / mi.haystack_len + 1.0 / needle_len) / 2.0;
        mi.level1 = level1;
        mi.level2 = level2;
        mi.level3 = level3;
        mi.cache = cache;
        haystack[i].score = score_item(&mi, haystack[i].positions, stack, needle_len, max_haystack_len, posbuf);
    }

    stack = free_stack(stack);
    cache = free_cache(cache);
    free(posbuf); posbuf = NULL;
    return ret;
}


static int 
read_stdin(char *needle, args_info *opts) {
    char *linebuf = NULL;
    size_t n = 0, needle_len = strlen(needle), max_haystack_len = 0, idx = 0;
    ssize_t sz = 0;
    int ret = 0;
    Positions positions = {0};
    Candidates candidates = {0};
    Chars chars = {0};

    if (needle_len < 1) { fprintf(stderr, "Empty query not allowed.\n"); return 1; }
    ALLOC_VEC(char, chars, 8192 * 20);
    ALLOC_VEC(int32_t, positions, 8192 * needle_len);
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
                ENSURE_SPACE(int32_t, positions, needle_len);
                ENSURE_SPACE(Candidate, candidates, 1);
                memcpy(&NEXT(chars), linebuf, sz);
                NEXT(candidates).src = &NEXT(chars);
                NEXT(candidates).src_sz = sz - 1;
                NEXT(candidates).positions = &NEXT(positions);
                NEXT(candidates).idx = idx++;
                INC(candidates, 1); INC(chars, sz); INC(positions, needle_len);
            }
        }
    }

    Candidate *haystack = &ITEM(candidates, 0);

    ret = run_scoring(haystack, 0, SIZE(candidates), needle, needle_len, max_haystack_len, opts->level1_arg, opts->level2_arg, opts->level3_arg);
    output_results(haystack, SIZE(candidates), opts, needle_len);

    if (linebuf) free(linebuf);
    linebuf = NULL;
    FREE_VEC(chars); FREE_VEC(positions); FREE_VEC(candidates);
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
    if (opts.mark_before_arg) unescape(opts.mark_before_arg, mark_before, sizeof(mark_before) - 1);
    if (opts.mark_after_arg) unescape(opts.mark_after_arg, mark_after, sizeof(mark_before) - 1);
    needle = opts.inputs[0];
    ret = read_stdin(needle, &opts);

end:
    cmdline_parser_free(&opts);
    return ret;
}

