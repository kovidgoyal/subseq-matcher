/*
 * windows_compat.c
 * Copyright (C) 2017 Kovid Goyal <kovid at kovidgoyal.net>
 *
 * Distributed under terms of the GPL3 license.
 */

#include "data-types.h"

#include <windows.h>
#include <process.h>

int 
number_of_processors() {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
}

void*
alloc_threads(size_t num_threads) {
    return calloc(num_threads, sizeof(uintptr_t));
}

bool
start_thread(void* vt, size_t i, unsigned int (*start_routine) (void *), void *arg) {
    uintptr_t *threads = (uintptr_t*)vt;
    errno = 0;
    threads[i] = _beginthreadex(NULL, 0, start_routine, arg, 0, NULL);
    if (threads[i] == 0) {
        perror("Failed to create thread, with error");
        return false;
    }
    return true;
}

void
wait_for_thread(void *vt, size_t i) {
    uintptr_t *threads = vt;
    WaitForSingleObject((HANDLE)threads[i], INFINITE);
    CloseHandle((HANDLE)threads[i]);
    threads[i] = 0;
}

void
free_threads(void *threads) {
    free(threads);
}
