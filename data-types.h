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

typedef struct {
    char* src;
    ssize_t src_sz;
    int32_t *positions;
    double score;
    ssize_t idx;
} Candidate;

VECTOR_OF(int32_t, Positions);
VECTOR_OF(char, Chars);
VECTOR_OF(Candidate, Candidates);


typedef struct {
    double score;
    int32_t *positions;
} CacheItem;


typedef struct {
    size_t hidx;
    size_t nidx;
    size_t last_idx;
    double score;
    int32_t *positions;
} StackItem;

typedef struct {
    ssize_t pos;
    int32_t needle_len;
    size_t size;
    StackItem *items;
} Stack;


typedef struct {
    char *haystack;
    int32_t haystack_len;
    char *needle;
    int32_t needle_len;
    double max_score_per_char;
    CacheItem ***cache;
    char *level1;
    char *level2;
    char *level3;
} MatchInfo;



Stack* alloc_stack(int32_t needle_len, int32_t max_haystack_len);
Stack* free_stack(Stack *stack);
CacheItem*** alloc_cache(int32_t needle_len, int32_t max_haystack_len);
CacheItem*** free_cache(CacheItem ***);
double score_item(MatchInfo *mi, int32_t *positions, Stack *stack, int32_t, int32_t, int32_t*);
