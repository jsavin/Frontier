#ifndef FRONTIER_TIME_PORTABLE_H
#define FRONTIER_TIME_PORTABLE_H

#include <stdint.h>

#define FRONTIER_TIMESTAMP_FLAG_DST 0x0001

typedef struct frontier_timestamp_v7 {
    uint64_t unix_ms;
    int32_t tz_minutes;
    uint16_t flags;
} frontier_timestamp_v7;

uint64_t frontier_time_wallclock_millis(void);
uint64_t frontier_time_monotonic_millis(void);
uint64_t frontier_time_monotonic_micros(void);
int32_t frontier_time_local_offset_minutes(void);
void frontier_time_snapshot(frontier_timestamp_v7 *out_timestamp);
void frontier_time_sleep_millis(uint32_t millis);
uint32_t frontier_time_ticks(void);

#define FastMilliseconds() ((long)frontier_time_wallclock_millis())

#endif

