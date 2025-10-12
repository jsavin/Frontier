#ifndef FRONTIER_TIME_PORTABLE_H
#define FRONTIER_TIME_PORTABLE_H

#include <stdint.h>
#include <time.h>

static inline uint64_t frontier_milliseconds_now(void){
#if defined(CLOCK_MONOTONIC)
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec*1000ULL + (uint64_t)ts.tv_nsec/1000000ULL;
#else
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec*1000ULL + (uint64_t)ts.tv_nsec/1000000ULL;
#endif
}

#define FastMilliseconds() ((long)frontier_milliseconds_now())

#endif


