/*
 * output.c
 * Copyright (C) 2017 Kovid Goyal <kovid at kovidgoyal.net>
 *
 * Distributed under terms of the GPL3 license.
 */

#include "data-types.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static text_t mark_before[100] = {0}, mark_after[100] = {0};

static void
unescape(text_t *src, text_t *dest, size_t destlen) {
    size_t srclen = strlen(src);
    text_t buf[5] = {0};

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
                        dest[j] = (text_t)(strtol(buf, NULL, 16));
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
output_with_marks(text_t *src, len_t *positions, len_t poslen) {
    unsigned int i, pos, j = 0;
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
output_result(Candidate *c, args_info *opts, len_t needle_len) {
    UNUSED(opts);
    if (c->score > 0.0 && (mark_before[0] || mark_after[0])) {
        output_with_marks(c->src, c->positions, needle_len);
    } else printf("%s\n", c->src);
}


void
output_results(Candidate *haystack, size_t count, args_info *opts, len_t needle_len) {
    Candidate *c;
    qsort(haystack, count, sizeof(*haystack), cmpscore);
    size_t left = opts->limit_arg > 0 ? (size_t)opts->limit_arg : count;
    if (opts->mark_before_arg) unescape(opts->mark_before_arg, mark_before, sizeof(mark_before) - 1);
    if (opts->mark_after_arg) unescape(opts->mark_after_arg, mark_after, sizeof(mark_before) - 1);
    for (size_t i = 0; i < left; i++) {
        c = haystack + i;
        if (c->score > 0) output_result(c, opts, needle_len);
    }
}



