/* platform_adapter.h - Abstract platform services for portable core */

#ifndef FRONTIER_PLATFORM_ADAPTER_H
#define FRONTIER_PLATFORM_ADAPTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct frontier_timeval {
    int64_t millis;
} frontier_timeval;

/* File IO */
bool fp_read_all(const char* path, void** out_data, size_t* out_size);
bool fp_write_all(const char* path, const void* data, size_t size);
bool fp_exists(const char* path);

/* Time */
frontier_timeval fp_now(void);

/* Logging */
void fp_log(const char* message);

#endif /* FRONTIER_PLATFORM_ADAPTER_H */


