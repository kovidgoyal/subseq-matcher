/*
 * score.c
 * Copyright (C) 2017 Kovid Goyal <kovid at kovidgoyal.net>
 *
 * Distributed under terms of the GPL3 license.
 */

#include "data-types.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>

CacheItem***
alloc_cache(int32_t needle_len, int32_t max_haystack_len) {
    CacheItem ***ans = NULL, **d1 = NULL, *d2 = NULL;
    size_t num = max_haystack_len * max_haystack_len * needle_len;
    size_t position_sz = needle_len * sizeof(int32_t);
    size_t sz = (num * (sizeof(CacheItem) + position_sz)) + (max_haystack_len * sizeof(CacheItem**)) + (needle_len * sizeof(CacheItem*));
    int32_t hidx, nidx, last_idx, i, j;
    char *base = NULL;

    ans = (CacheItem***) calloc(sz, 1);
    if (ans != NULL) {
        d1 = (CacheItem**)(ans + max_haystack_len);
        d2 = (CacheItem*) (d1 + max_haystack_len * needle_len );
        for (i = 0; i < max_haystack_len; i++) {
            ans[i] = d1 + i * needle_len;
            for (j = 0; j < needle_len; j++) d1[i*needle_len + j] = d2 + j;
        }

        base = ((char*)ans) + (sizeof(CacheItem**)*max_haystack_len) + (sizeof(CacheItem*)*needle_len) + (sizeof(CacheItem)*max_haystack_len);

        for (hidx = 0; hidx < max_haystack_len; hidx++) {
            for (nidx = 0; nidx < needle_len; nidx++) {
                for (last_idx = 0; last_idx < max_haystack_len; last_idx++) {
                    ans[hidx][nidx][last_idx].positions = (int32_t*)base;
                    base += position_sz;
                }
            }
        }
    }
    return ans;
}


CacheItem***
free_cache(CacheItem*** c) { if (c) free(c); return NULL; }


Stack* 
alloc_stack(int32_t needle_len, int32_t max_haystack_len) {
    StackItem *ans = NULL;
    char *base = NULL;
    size_t num = max_haystack_len * needle_len;
    size_t position_sz = needle_len * sizeof(int32_t);
    size_t sz = sizeof(StackItem) + position_sz;
    size_t i = 0;
    Stack *stack = (Stack*)calloc(1, sizeof(Stack));
    if (stack == NULL) return NULL;

    stack->needle_len = needle_len;
    stack->pos = -1;
    stack->size = num;
    ans = (StackItem*) calloc(num, sz);
    if (ans != NULL) {
        base = (char*)(ans + num);
        for (i = 0; i < num; i++, base += position_sz) ans[i].positions = (int32_t*) base;
        stack->items = ans;
    } else { free(stack); return NULL; }
    return stack;
}

Stack*
free_stack(Stack *stack) {
    if (stack->items) free(stack->items);
    stack->items = NULL;
    free(stack);
    return NULL;
}


static inline void 
clear_cache(CacheItem ***mem, int32_t needle_len, int32_t max_haystack_len) {
    int32_t hidx, nidx, last_idx;
    for (hidx = 0; hidx < max_haystack_len; hidx++) {
        for (nidx = 0; nidx < needle_len; nidx++) {
            for (last_idx = 0; last_idx < max_haystack_len; last_idx++) {
                mem[hidx][nidx][last_idx].score = DBL_MAX;
            }
        }
    }
}

static inline void 
clear_stack(Stack *stack) { stack->pos = -1; }

static inline void 
stack_push(Stack *stack, int32_t hidx, int32_t nidx, int32_t last_idx, double score, int32_t *positions) {
    StackItem *si = &(stack->items[++stack->pos]);
    si->hidx = hidx; si->nidx = nidx; si->last_idx = last_idx; si->score = score;
    memcpy(si->positions, positions, sizeof(*positions) * stack->needle_len);
}

static inline void 
stack_pop(Stack *stack, int32_t *hidx, int32_t *nidx, int32_t *last_idx, double *score, int32_t *positions) {
    StackItem *si = &(stack->items[stack->pos--]);
    *hidx = si->hidx; *nidx = si->nidx; *last_idx = si->last_idx; *score = si->score;
    memcpy(positions, si->positions, sizeof(*positions) * stack->needle_len);
}

double
score_item(MatchInfo *mi, int32_t *positions, CacheItem ***cache, Stack *stack, int32_t needle_len, int32_t max_haystack_len) {
    clear_stack(stack);
    clear_cache(cache, needle_len, max_haystack_len);
    return 0;
}
