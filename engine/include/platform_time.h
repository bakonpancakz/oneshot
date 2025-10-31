#pragma once

#ifdef _WIN32
#include <windows.h>

static inline void time_get(unsigned int* h, unsigned int* m, unsigned int* s, unsigned int* ms) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    *h = (unsigned int)t.wHour;
    *m = (unsigned int)t.wMinute;
    *s = (unsigned int)t.wSecond;
    *ms = (unsigned int)t.wMilliseconds;
}

#else
#error "Platform Time Not Implemented"
#endif