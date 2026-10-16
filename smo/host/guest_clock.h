#pragma once
#include <stdint.h>
#include <time.h>

// Counter instructions and GetSystemTick share the monotonic host epoch.
// Splitting seconds avoids overflowing a nanosecond product.
static inline uint64_t dm_tick_from_timespec(const struct timespec *ts) {
    return (uint64_t)ts->tv_sec * 19200000ull + (uint64_t)ts->tv_nsec * 12 / 625;
}
static inline uint64_t dm_guest_tick(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return dm_tick_from_timespec(&ts);
}
