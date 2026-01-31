// portable/time_portable.c
// 2025-11-09 Codex (GPT-5): Provide a cross-platform time layer for headless builds.

#define _GNU_SOURCE

#include "time_portable.h"

#include <errno.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#define WINDOWS_TICKS_PER_MS 10000ULL
#define WINDOWS_EPOCH_BIAS 116444736000000000ULL /* 1601 -> 1970 in 100ns units */

static uint64_t frontier_time_from_timespec(const struct timespec *ts) {
    return (uint64_t)ts->tv_sec * 1000ULL + (uint64_t)ts->tv_nsec / 1000000ULL;
}

static uint64_t frontier_time_from_timespec_us(const struct timespec *ts) {
    return (uint64_t)ts->tv_sec * 1000000ULL + (uint64_t)ts->tv_nsec / 1000ULL;
}

#if defined(_WIN32)
static double frontier_time_perf_to_millis = 0.0;
static double frontier_time_perf_to_micros = 0.0;

static void frontier_time_ensure_perf_scale(void) {
    if (frontier_time_perf_to_millis != 0.0)
        return;

    LARGE_INTEGER freq;
    if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0)
        return;

    frontier_time_perf_to_millis = 1000.0 / (double)freq.QuadPart;
    frontier_time_perf_to_micros = 1000000.0 / (double)freq.QuadPart;
}
#endif

static void frontier_time_get_local_tm(time_t now, struct tm *local_tm) {
    if (local_tm == NULL)
        return;
#if defined(_WIN32)
    struct tm temp;
    errno_t err = localtime_s(&temp, &now);
    if (err == 0)
        *local_tm = temp;
#elif defined(_POSIX_THREAD_SAFE_FUNCTIONS)
    localtime_r(&now, local_tm);
#else
    struct tm *tmp = localtime(&now);
    if (tmp != NULL)
        *local_tm = *tmp;
#endif
}

uint64_t frontier_time_wallclock_millis(void) {
#if defined(_WIN32)
    FILETIME ft;
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0601
    GetSystemTimePreciseAsFileTime(&ft);
#else
    GetSystemTimeAsFileTime(&ft);
#endif
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    uint64_t unix_100ns = uli.QuadPart - WINDOWS_EPOCH_BIAS;
    return unix_100ns / WINDOWS_TICKS_PER_MS;
#else
#if defined(CLOCK_REALTIME)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
        return frontier_time_from_timespec(&ts);
#endif
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
#endif
}

uint64_t frontier_time_monotonic_millis(void) {
#if defined(_WIN32)
    frontier_time_ensure_perf_scale();
    if (frontier_time_perf_to_millis == 0.0)
        return frontier_time_wallclock_millis();
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * frontier_time_perf_to_millis);
#else
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return frontier_time_from_timespec(&ts);
#endif
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
#endif
}

uint64_t frontier_time_monotonic_micros(void) {
#if defined(_WIN32)
    frontier_time_ensure_perf_scale();
    if (frontier_time_perf_to_micros == 0.0)
        return frontier_time_wallclock_millis() * 1000ULL;
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * frontier_time_perf_to_micros);
#else
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return frontier_time_from_timespec_us(&ts);
#endif
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
#endif
}

int32_t frontier_time_local_offset_minutes(void) {
    time_t now = time(NULL);
#if defined(_WIN32)
    TIME_ZONE_INFORMATION info;
    DWORD state = GetTimeZoneInformation(&info);
    LONG bias = info.Bias;
    if (state == TIME_ZONE_ID_STANDARD)
        bias += info.StandardBias;
    else if (state == TIME_ZONE_ID_DAYLIGHT)
        bias += info.DaylightBias;
    return (int32_t)(-bias);
#else
    struct tm local_tm;
    memset(&local_tm, 0, sizeof(local_tm));
    frontier_time_get_local_tm(now, &local_tm);
#if defined(__APPLE__) || defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    return (int32_t)(local_tm.tm_gmtoff / 60);
#else
    tzset();
#if defined(HAVE_DECL_TIMEZONE) || defined(__GLIBC__)
    extern long timezone;
    long seconds_west = timezone;
    if (local_tm.tm_isdst > 0)
        seconds_west -= 3600;
    return (int32_t)(-seconds_west / 60);
#else
    (void)local_tm;
    return 0;
#endif
#endif
#endif
}

static uint16_t frontier_time_current_flags(void) {
    uint16_t flags = 0;
    time_t now = time(NULL);
    struct tm local_tm;
    memset(&local_tm, 0, sizeof(local_tm));
    frontier_time_get_local_tm(now, &local_tm);
    if (local_tm.tm_isdst > 0)
        flags |= FRONTIER_TIMESTAMP_FLAG_DST;
    return flags;
}

void frontier_time_snapshot(frontier_timestamp_v7 *out_timestamp) {
    if (out_timestamp == NULL)
        return;
    frontier_timestamp_v7 snapshot;
    snapshot.unix_ms = frontier_time_wallclock_millis();
    snapshot.tz_minutes = frontier_time_local_offset_minutes();
    snapshot.flags = frontier_time_current_flags();
    *out_timestamp = snapshot;
}

void frontier_time_sleep_millis(uint32_t millis) {
#if defined(_WIN32)
    Sleep(millis);
#else
    struct timespec req;
    req.tv_sec = millis / 1000U;
    req.tv_nsec = (long)(millis % 1000U) * 1000000L;
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
        continue;
    }
#endif
}

/*
 * TickCount - Mac-compatible tick counter for headless/portable builds
 *
 * Returns time in 1/60th second intervals (Mac "ticks").
 * This is the production implementation - no test harness dependencies.
 */
uint32_t frontier_time_ticks(void) {
    uint64_t ms = frontier_time_monotonic_millis();
    // Convert milliseconds to 60ths of a second: ms * 60 / 1000 = ms * 3 / 50
    return (uint32_t)((ms * 3ULL) / 50ULL);
}
