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
#ifdef ISWINDOWS
#include <io.h>
#define STDOUT_FILENO 1
static inline ssize_t ms_write(int fd, const void* buf, size_t count) { return _write(fd, buf, (unsigned int)count); }
#define write ms_write
#else
#include <unistd.h>
#endif
#include <errno.h>

static char mark_before[100] = {0}, mark_after[100] = {0};
static size_t mark_before_sz = 0, mark_after_sz = 0;

size_t
unescape(char *src, char *dest, size_t destlen) {
    size_t srclen = strlen(src), i, j;
    char buf[5] = {0};

    for (i = 0, j = 0; i < MIN(srclen, destlen); i++, j++) {
        if (src[i] == '\\' && i < srclen - 1) {
            switch(src[++i]) {
#define S(x) dest[j] = x; break;
                case 'e':
                case 'E':
                    S(0x1b);
                case 'a':
                    S('\a');
                case 'b':
                    S('\b');
                case 'f':
                    S('\f');
                case 'n':
                    S('\n');
                case 'r':
                    S('\r');
                case 't':
                    S('\t');
                case 'v':
                    S('\v');
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
    return j;
}


#define FIELD(x, which) (((Candidate*)(x))->which)

static int 
cmpscore(const void *a, const void *b) {
    double sa = FIELD(a, score), sb = FIELD(b, score);
    // Sort descending
    return (sa > sb) ? -1 : ((sa == sb) ? ((int)FIELD(a, idx) - (int)FIELD(b, idx)) : 1);
}

#define BUF_CAPACITY 16384
static char write_buf[BUF_CAPACITY] = {0};
static size_t write_buf_sz = 0;

static void
eintr_write() {
    ssize_t ret;
    char *buf = write_buf;
    while (write_buf_sz > 0) {
        errno = 0;
        ret = write(STDOUT_FILENO, buf, write_buf_sz);
        if (ret <= 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("Could not write to output"); exit(1); 
        }
        buf += ret;
        write_buf_sz -= ret;
    }
}


static void
buffered_write(const char *buf, size_t sz) {
    if (sz + write_buf_sz < BUF_CAPACITY) {
        memcpy(write_buf + write_buf_sz, buf, sz);
        write_buf_sz += sz;
    } else {
        eintr_write();
        if (sz > BUF_CAPACITY) { fprintf(stderr, "Cannot write in large chunk sizes\n"); exit(1); }
        buffered_write(buf, sz);
    }
}

static void
write_text(text_t *text, size_t sz) {
    static char buf[10] = {0};
    for (size_t i = 0; i < sz; i++) {
        unsigned int num = encode_codepoint(text[i], buf);
        if (num > 0) buffered_write(buf, num);
    }
}

static void
output_with_marks(text_t *src, size_t src_sz, len_t *positions, len_t poslen) {
    size_t pos, i = 0;
    for (pos = 0; pos < poslen; pos++, i++) {
        write_text(src + i, MIN(src_sz, positions[pos]) - i);
        i = positions[pos];
        if (i < src_sz) {
            if (mark_before_sz > 0) buffered_write(mark_before, mark_before_sz);
            write_text(src + i, 1);
            if (mark_after_sz > 0) buffered_write(mark_after, mark_after_sz);
        }
    }
    i = positions[poslen - 1];
    if (i + 1 < src_sz) write_text(src + i + 1, src_sz - i - 1);
}

static void
output_positions(len_t *positions, len_t num) {
    static char pbuf[100] = {0};
    for (len_t i = 0; i < num; i++) {
        buffered_write(pbuf, snprintf(pbuf, sizeof(pbuf), "%u%s", positions[i], (i == num - 1) ? ":" : ","));
    }
}


static void
output_result(Candidate *c, args_info *opts, len_t needle_len, char delim) {
    UNUSED(opts);
    if (opts->positions_flag) output_positions(c->positions, needle_len);
    if (mark_before_sz > 0 || mark_after_sz > 0) {
        output_with_marks(c->src, c->src_sz, c->positions, needle_len);
    } else {
        write_text(c->src, c->src_sz);
    }
    buffered_write(&delim, 1);
}


void
output_results(Candidate *haystack, size_t count, args_info *opts, len_t needle_len, char delim) {
    Candidate *c;
    qsort(haystack, count, sizeof(*haystack), cmpscore);
    size_t left = opts->limit_arg > 0 ? (size_t)opts->limit_arg : count;
    if (opts->mark_before_arg) mark_before_sz = unescape(opts->mark_before_arg, mark_before, sizeof(mark_before) - 1);
    if (opts->mark_after_arg) mark_after_sz = unescape(opts->mark_after_arg, mark_after, sizeof(mark_before) - 1);
    for (size_t i = 0; i < left; i++) {
        c = haystack + i;
        if (c->score > 0) output_result(c, opts, needle_len, delim);
    }
    if (write_buf_sz > 0) eintr_write();
}
