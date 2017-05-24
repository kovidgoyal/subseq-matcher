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
#include <string.h>
#include <ctype.h>

#ifndef ISWINDOWS
#include <unistd.h>
#include <pthread.h>
#endif

#ifdef __APPLE__
#ifndef _SC_NPROCESSORS_ONLN
#define _SC_NPROCESSORS_ONLN 58
#endif
#endif

typedef struct {
    size_t start, count;
    void *workspace;
    len_t max_haystack_len;
    bool started;
} JobData;


static GlobalData global = {0};

static void 
run_scoring(JobData *job_data) {
    for (size_t i = job_data->start; i < job_data->start + job_data->count; i++) {
        global.haystack[i].score = score_item(job_data->workspace, global.haystack[i].src, global.haystack[i].haystack_len, global.haystack[i].positions);
    }

}

static void*
run_scoring_threaded(void *job_data) {
    run_scoring((JobData*)job_data);
    return NULL;
}

static JobData*
create_job(size_t i, size_t blocksz) {
    JobData *ans = (JobData*)calloc(1, sizeof(JobData));
    if (ans == NULL) return NULL;
    ans->start = i * blocksz;
    if (ans->start >= global.haystack_count) ans->count = 0;
    else ans->count = global.haystack_count - ans->start;
    ans->max_haystack_len = 0;
    for (size_t i = ans->start; i < ans->start + ans->count; i++) ans->max_haystack_len = MAX(ans->max_haystack_len, global.haystack[i].haystack_len);
    if (ans->count > 0) {
        ans->workspace = alloc_workspace(ans->max_haystack_len, &global);
        if (!ans->workspace) { free(ans); return NULL; }
    }
    return ans;
}

static JobData*
free_job(JobData *job) {
    if (job) {
        if (job->workspace) free_workspace(job->workspace);
        free(job);
    }
    return NULL;
}

static int
run_threaded(int num_threads_asked) {
    int ret = 0, rc;
    size_t i, blocksz;
    size_t num_threads = MAX(1, num_threads_asked > 0 ? num_threads_asked : sysconf(_SC_NPROCESSORS_ONLN));
    if (global.haystack_size < 10000) num_threads = 1;
    /* printf("num_threads: %lu asked: %d sysconf: %ld\n", num_threads, num_threads_asked, sysconf(_SC_NPROCESSORS_ONLN)); */

    pthread_t *threads = calloc(num_threads, sizeof(pthread_t));
    JobData **job_data = calloc(num_threads, sizeof(JobData*));
    if (threads == NULL || job_data == NULL) { ret = 1; goto end; }

    blocksz = global.haystack_count / num_threads + global.haystack_count % num_threads;

    for (i = 0; i < num_threads; i++) {
        job_data[i] = create_job(i, blocksz);
        if (job_data[i] == NULL) { ret = 1; goto end; }
    }

    if (num_threads == 1) {
        run_scoring(job_data[0]);
    } else {
        for (i = 0; i < num_threads; i++) {
            job_data[i]->started = false;
            if (job_data[i]->count > 0) {
                if ((rc = pthread_create(threads + i, NULL, run_scoring_threaded, job_data[i]))) {
                    fprintf(stderr, "Failed to create thread, with error: %s\n", strerror(rc));
                    ret = 1;
                } else { job_data[i]->started = true; }
            }
        }
    }

end:
    if (num_threads > 1 && job_data) {
        for (i = 0; i < num_threads; i++) {
            if (job_data[i] && job_data[i]->started) pthread_join(threads[i], NULL);
        }
    }
    for (i = 0; i < num_threads; i++) job_data[i] = free_job(job_data[i]);
    free(job_data);
    free(threads); 
    return ret;
}

static int 
read_stdin(args_info *opts, char delimiter) {
    char *linebuf = NULL;
    size_t n = 0, idx = 0;
    ssize_t sz = 0;
    int ret = 0;
    Candidates candidates = {0};
    Chars chars = {0};

    ALLOC_VEC(text_t, chars, 8192 * 20);
    ALLOC_VEC(Candidate, candidates, 8192);
    if (chars.data == NULL || candidates.data == NULL) return 1;

    while (true) {
        errno = 0;
        sz = getdelim(&linebuf, &n, delimiter, stdin);
        if (sz < 1) {
            if (errno != 0) {
                perror("Failed to read from STDIN with error:"); 
                ret = 1;
            }
            break;
        }
        if (sz > 1) {
            if (linebuf[sz - 1] == '\n') linebuf[--sz] = 0;
            if (sz > 0) {
                ENSURE_SPACE(text_t, chars, sz);
                ENSURE_SPACE(Candidate, candidates, 1);
                sz = decode_string(linebuf, sz, &(NEXT(chars)));
                NEXT(candidates).src_sz = sz;
                NEXT(candidates).haystack_len = (len_t)(MIN(LEN_MAX, sz));
                global.haystack_size += NEXT(candidates).haystack_len;
                NEXT(candidates).idx = idx++;
                INC(candidates, 1); INC(chars, sz); 
            }
        }
    }

    // Prepare the haystack allocating space for positions arrays and settings
    // up the src pointers to point to the correct locations
    Candidate *haystack = &ITEM(candidates, 0);
    len_t *positions = (len_t*)calloc(SIZE(candidates), sizeof(len_t) * global.needle_len);
    if (positions) {
        text_t *cdata = &ITEM(chars, 0);
        for (size_t i = 0, off = 0; i < SIZE(candidates); i++) {
            haystack[i].positions = positions + (i * global.needle_len);
            haystack[i].src = cdata + off;
            off += haystack[i].src_sz;
        }
        global.haystack = haystack;
        global.haystack_count = SIZE(candidates);
        ret = run_threaded(opts->threads_arg);
        if (ret == 0) output_results(haystack, SIZE(candidates), opts, global.needle_len);
        else { REPORT_OOM; }
    } else { ret = 1; REPORT_OOM; }

    if (linebuf) free(linebuf);
    linebuf = NULL;
    FREE_VEC(chars); free(positions); FREE_VEC(candidates);
    return ret;
}

#define SET_TEXT_ARG(src, name, ui_name) \
    arglen = strlen(src); \
    if (arglen > LEN_MAX) { \
        fprintf(stderr, "The %s must be no longer than %d bytes\n", ui_name, LEN_MAX); \
        ret = 1; goto end; \
    } \
    global.name##_len = (len_t)decode_string(src, arglen, global.name);

int 
main(int argc, char *argv[]) {
    args_info opts;
    int ret = 0;
    size_t arglen;
    char delimiter[10] = {0};

    if (cmdline_parser(argc, argv, &opts) != 0) return 1;
    if (opts.inputs_num != 1) {
        fprintf(stderr, "You must specify a single query\n");
        ret = 1; 
        goto end;
    }
    SET_TEXT_ARG(opts.inputs[0], needle, "query");
    SET_TEXT_ARG(opts.level1_arg, level1, "level1 string");
    SET_TEXT_ARG(opts.level2_arg, level2, "level2 string");
    SET_TEXT_ARG(opts.level3_arg, level3, "level3 string");
    if (global.needle_len < 1) { fprintf(stderr, "Empty query not allowed.\n"); ret = 1; goto end; }
    if (opts.delimiter_arg) unescape(opts.delimiter_arg, delimiter, 5);
    else delimiter[0] = '\n';
    ret = read_stdin(&opts, delimiter[0]);

end:
    cmdline_parser_free(&opts);
    return ret;
}

