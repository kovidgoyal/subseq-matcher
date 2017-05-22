/*
 * Copyright (C) 2017 Kovid Goyal <kovid at kovidgoyal.net>
 *
 * Distributed under terms of the GPL3 license.
 */

#pragma once
#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <sys/types.h>

typedef struct {
    const char* src;
    ssize_t src_sz;
} Candidate;

typedef 
