#ifndef NV0KEN_LIB_LOG_H
#define NV0KEN_LIB_LOG_H

typedef enum {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
} log_level_t;

void log_set_level(log_level_t level);
log_level_t log_get_level(void);
void log_write(log_level_t level, const char *subsystem, const char *fmt, ...);

#define log_trace(subsystem, fmt, ...) log_write(LOG_TRACE, subsystem, fmt, ##__VA_ARGS__)
#define log_debug(subsystem, fmt, ...) log_write(LOG_DEBUG, subsystem, fmt, ##__VA_ARGS__)
#define log_info(subsystem, fmt, ...)  log_write(LOG_INFO,  subsystem, fmt, ##__VA_ARGS__)
#define log_warn(subsystem, fmt, ...)  log_write(LOG_WARN,  subsystem, fmt, ##__VA_ARGS__)
#define log_error(subsystem, fmt, ...) log_write(LOG_ERROR, subsystem, fmt, ##__VA_ARGS__)
#define log_fatal(subsystem, fmt, ...) log_write(LOG_FATAL, subsystem, fmt, ##__VA_ARGS__)

#endif
