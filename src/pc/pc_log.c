#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include "pc_log.h"
#include "platform.h"

static FILE *log_file = NULL;

void pc_log_init(void) {
    if (log_file) return;

    const char *user_path = sys_user_path();
    char log_path[SYS_MAX_PATH];
    snprintf(log_path, sizeof(log_path), "%s/game.log", user_path);

    log_file = fopen(log_path, "a");
    if (log_file) {
        LOG_INFO("--- Log Session Started ---");
    } else {
        fprintf(stderr, "Could not open log file: %s\n", log_path);
    }
}

void pc_log_print(LogLevel level, const char *fmt, ...) {
    static const char *level_strs[] = { "INFO", "ERROR", "FATAL" };
    
    va_list args;
    time_t rawtime;
    struct tm *timeinfo;
    char time_str[20];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Print to stdout/stderr
    va_start(args, fmt);
    if (level == LOG_LEVEL_INFO) {
        printf("[%s] [%s] ", time_str, level_strs[level]);
        vprintf(fmt, args);
        printf("\n");
    } else {
        fprintf(stderr, "[%s] [%s] ", time_str, level_strs[level]);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
    }
    va_end(args);

    // Print to file
    if (log_file) {
        fprintf(log_file, "[%s] [%s] ", time_str, level_strs[level]);
        va_start(args, fmt);
        vfprintf(log_file, fmt, args);
        va_end(args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }
}

void pc_log_shutdown(void) {
    if (log_file) {
        LOG_INFO("--- Log Session Ended ---");
        fclose(log_file);
        log_file = NULL;
    }
}
