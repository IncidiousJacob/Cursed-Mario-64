#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <execinfo.h>
#include "pc_debug.h"
#include "pc_log.h"

void pc_print_backtrace(void) {
    void *array[32];
    size_t size;
    char **strings;

    size = backtrace(array, 32);
    strings = backtrace_symbols(array, size);

    LOG_ERROR("Backtrace (%zu frames):", size);
    for (size_t i = 0; i < size; i++) {
        LOG_ERROR("  %s", strings[i]);
    }

    free(strings);
}

static void sigusr1_handler(int sig) {
    LOG_ERROR("Caught signal %d (Watchdog Hang Detection)", sig);
    pc_print_backtrace();
    LOG_ERROR("Terminating due to hang...");
    exit(1);
}

void pc_debug_init(void) {
    signal(SIGUSR1, sigusr1_handler);
}
