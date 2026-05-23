#include "log.h"

#include <stdarg.h>

#include "kprintf.h"

static log_level_t current_level = LOG_INFO;

static const char *level_name(log_level_t level)
{
    switch (level) {
    case LOG_TRACE: return "trace";
    case LOG_DEBUG: return "debug";
    case LOG_INFO:  return "info";
    case LOG_WARN:  return "warn";
    case LOG_ERROR: return "error";
    case LOG_FATAL: return "fatal";
    default:        return "log";
    }
}

void log_set_level(log_level_t level)
{
    current_level = level;
}

log_level_t log_get_level(void)
{
    return current_level;
}

void log_write(log_level_t level, const char *subsystem, const char *fmt, ...)
{
    if (level < current_level) {
        return;
    }

    kprintf("[%s", level_name(level));
    if (subsystem && subsystem[0]) {
        kprintf(":%s", subsystem);
    }
    kprintf("] ");

    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
    kprintf("\n");
}
