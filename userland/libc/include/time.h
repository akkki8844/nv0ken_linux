#ifndef NV0_TIME_H
#define NV0_TIME_H

#include <sys/types.h>

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

int nanosleep(const struct timespec *request, struct timespec *remaining);
int clock_gettime(int clock_id, struct timespec *value);
time_t time(time_t *result);

#endif
