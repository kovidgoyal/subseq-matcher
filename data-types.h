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
    int32_t *positions, *needlebuf;
    double score;
} Candidate;

VECTOR_OF(int32_t, Positions);
VECTOR_OF(char, Chars);
VECTOR_OF(Candidate, Candidates);
