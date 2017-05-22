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
stack_push(Stack *stack, size_t hidx, size_t nidx, size_t last_idx, double score, int32_t *positions) {
    StackItem *si = &(stack->items[++stack->pos]);
    si->hidx = hidx; si->nidx = nidx; si->last_idx = last_idx; si->score = score;
    memcpy(si->positions, positions, sizeof(*positions) * stack->needle_len);
}

static inline void 
stack_pop(Stack *stack, size_t *hidx, size_t *nidx, size_t *last_idx, double *score, int32_t *positions) {
    StackItem *si = &(stack->items[stack->pos--]);
    *hidx = si->hidx; *nidx = si->nidx; *last_idx = si->last_idx; *score = si->score;
    memcpy(positions, si->positions, sizeof(*positions) * stack->needle_len);
}

static double 
calc_score_for_char(MatchInfo *m, char last, char current, size_t distance_from_last_match) {
    double factor = 1.0;

    if (strchr(m->level1, last) != NULL) {
        factor = 0.9;
    } else if (strchr(m->level2, last) != NULL) {
        factor = 0.8;
    } else if ('a' <= last && last <= 'z' && 'A' <= current && current <= 'Z') {
        factor = 0.8;  // CamelCase
    } else if (strchr(m->level3, last) != NULL) {
        factor = 0.7;
    } else {
        // If last is not a special char, factor diminishes
        // as distance from last matched char increases
        factor = (1.0 / distance_from_last_match) * 0.75;
    }
    return m->max_score_per_char * factor;
}


static double 
process_item(MatchInfo *m, Stack *stack, int32_t *final_positions, int32_t *positions, int32_t needle_len) {
    char *pos;
    double final_score = 0.0, score = 0.0, score_for_char = 0.0;
    size_t i, hidx, nidx, last_idx, distance;
    CacheItem mem = {0};

    memset(final_positions, 0, sizeof(int32_t) * needle_len);
    stack_push(stack, 0, 0, 0, 0.0, final_positions);

    while (stack->pos >= 0) {
        stack_pop(stack, &hidx, &nidx, &last_idx, &score, positions);
        mem = m->cache[hidx][nidx][last_idx];
        if (mem.score == DBL_MAX) {
            // No memoized result, calculate the score
            for (i = nidx; i < m->needle_len; i++) {
                nidx = i; 
                if (m->haystack_len - hidx < m->needle_len - nidx) { score = 0.0; break; }
                pos = strchr(m->haystack + hidx, m->needle[nidx]);
                if (pos == NULL) { score = 0.0; break; } // No matches found
                distance = pos - (m->haystack + last_idx);
                if (distance <= 1) score_for_char = m->max_score_per_char;
                else score_for_char = calc_score_for_char(m, m->haystack[last_idx], *pos, distance);
                hidx++;
                if (m->haystack_len - hidx >= m->needle_len - nidx) stack_push(stack, hidx, nidx, last_idx, score, positions);
                last_idx = pos - m->haystack; 
                positions[nidx] = last_idx; 
                score += score_for_char;
            } // for(i) iterate over needle
            mem.score = score; memcpy(mem.positions, positions, sizeof(*positions) * m->needle_len);

        } else {
            score = mem.score; memcpy(positions, mem.positions, sizeof(*positions) * m->needle_len);
        }
        // We have calculated the score for this hidx, nidx, last_idx combination, update final_score and final_positions, if needed
        if (score > final_score) {
            final_score = score;
            memcpy(final_positions, positions, sizeof(*positions) * m->needle_len);
        }
    }
    return final_score;
}

double
score_item(MatchInfo *mi, int32_t *positions, Stack *stack, int32_t needle_len, int32_t max_haystack_len, int32_t *posbuf) {
    clear_stack(stack);
    clear_cache(mi->cache, needle_len, max_haystack_len);
    return process_item(mi, stack, positions, posbuf, needle_len);
}
