#include <errno.h>
#include <time.h>

extern long sys_time(time_t *result);
extern long sys_clock_gettime(int clock_id, struct timespec *value);
extern long sys_nanosleep(const struct timespec *request,
                          struct timespec *remaining);

time_t time(time_t *result)
{
    long value = sys_time(result);
    if (value < 0) {
        errno = (int)-value;
        return (time_t)-1;
    }
    if (result) {
        *result = (time_t)value;
    }
    return (time_t)value;
}

int clock_gettime(int clock_id, struct timespec *value)
{
    if (!value) {
        errno = EINVAL;
        return -1;
    }
    long result = sys_clock_gettime(clock_id, value);
    if (result < 0) {
        errno = (int)-result;
        return -1;
    }
    return 0;
}

int nanosleep(const struct timespec *request, struct timespec *remaining)
{
    if (!request || request->tv_nsec < 0 || request->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return -1;
    }
    long result = sys_nanosleep(request, remaining);
    if (result < 0) {
        errno = (int)-result;
        return -1;
    }
    return 0;
}
