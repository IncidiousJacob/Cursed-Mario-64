#ifndef PC_LOG_H
#define PC_LOG_H

#include <stdio.h>

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} LogLevel;

void pc_log_init(void);
void pc_log_print(LogLevel level, const char *fmt, ...);
void pc_log_shutdown(void);

#define LOG_INFO(...)  pc_log_print(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_ERROR(...) pc_log_print(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_FATAL(...) pc_log_print(LOG_LEVEL_FATAL, __VA_ARGS__)

#endif // PC_LOG_H
