/*
 * score.c
 * Copyright (C) 2017 Kovid Goyal <kovid at kovidgoyal.net>
 *
 * Distributed under terms of the GPL3 license.
 */

#include "data-types.h"
#include <stdlib.h>

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
