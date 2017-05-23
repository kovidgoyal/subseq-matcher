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
#include <pthread.h>

typedef struct {
    size_t start, count;
    void *workspace;
    len_t max_haystack_len;
    bool started;
} JobData;


typedef struct {
    Candidate *haystack;
    size_t haystack_count;
    char *level1, *level2, *level3, *needle;
    len_t needle_len;
} GlobalData;

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
    for (size_t i = ans->start; i < ans->start + global.haystack_count; i++) ans->max_haystack_len = MAX(ans->max_haystack_len, global.haystack[i].haystack_len);
    if (ans->count > 0) {
        ans->workspace = alloc_workspace(ans->max_haystack_len, global.needle_len, global.needle, global.level1, global.level2, global.level3);
        if (!ans->workspace) { free(ans); return NULL; }
    }
    return ans;
}

static void
free_job(JobData *job) {
    if (job) {
        if (job->workspace) free_workspace(job->workspace);
    }
}

static int
run_threaded(int num_threads) {
    int ret = 0, rc;
    size_t i, blocksz;
    num_threads = MAX(1, num_threads || sysconf(_SC_NPROCESSORS_ONLN));
    if (global.haystack_count < 100) num_threads = 1;

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
    for (i = 0; i < num_threads; i++) { if (job_data[i]) free_job(job_data[i]); }
    free(job_data);
    free(threads); 
    return ret;
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

    // Prepare the haystack allocating space for positions arrays and settings
    // up the src pointers to point to the correct locations
    Candidate *haystack = &ITEM(candidates, 0);
    len_t *positions = (len_t*)calloc(SIZE(candidates), sizeof(len_t) * needle_len);
    if (positions) {
        char *cdata = &ITEM(chars, 0);
        for (size_t i = 0, off = 0; i < SIZE(candidates); i++) {
            haystack[i].positions = positions + (i * needle_len);
            haystack[i].src = cdata + off;
            off += haystack[i].src_sz;
        }
        global.needle = needle; global.needle_len = needle_len;
        global.level1 = opts->level1_arg; global.level2 = opts->level2_arg;
        global.level3 = opts->level3_arg; global.haystack = haystack;
        global.haystack_count = SIZE(candidates);
        ret = run_threaded(opts->threads_arg);
        if (ret == 0) output_results(haystack, SIZE(candidates), opts, needle_len);
        else { REPORT_OOM; }
    } else { ret = 1; REPORT_OOM; }

    if (linebuf) free(linebuf);
    linebuf = NULL;
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

