/*
 * Copyright (C) 2017 Kovid Goyal <kovid at kovidgoyal.net>
 *
 * Distributed under terms of the GPL3 license.
 */

#pragma once
#define _POSIX_C_SOURCE 200809L

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>

#include "vector.h"
#include "cli.h"

typedef struct gengetopt_args_info args_info;

typedef uint8_t len_t;
typedef char text_t;

#define LEN_MAX UINT8_MAX
#define UNUSED(x) (void)(x)

typedef struct {
    text_t* src;
    ssize_t src_sz;
    len_t haystack_len;
    len_t *positions;
    double score;
    ssize_t idx;
} Candidate;

VECTOR_OF(len_t, Positions)
VECTOR_OF(text_t, Chars)
VECTOR_OF(Candidate, Candidates)


void output_results(Candidate *haystack, size_t count, args_info *opts, len_t needle_len);
void* alloc_workspace(len_t max_haystack_len, len_t needle_len, text_t *needle, text_t *level1, text_t *level2, text_t *level3);
void* free_workspace(void *v);
double score_item(void *v, text_t *haystack, len_t haystack_len, len_t *match_positions);
