#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#else
#include <execinfo.h>
#endif

#include "pc_debug.h"
#include "pc_log.h"

void pc_print_backtrace(void) {
#ifdef _WIN32
    void *stack[32];
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);

    unsigned short frames = CaptureStackBackTrace(0, 32, stack, NULL);
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)calloc(1, sizeof(SYMBOL_INFO) + 256 * sizeof(char));
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    LOG_ERROR("Backtrace (%d frames):", frames);
    for (unsigned short i = 0; i < frames; i++) {
        SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
        LOG_ERROR("  %d: %s - 0x%0X", i, symbol->Name, symbol->Address);
    }

    free(symbol);
#else
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
#endif
}

#ifndef _WIN32
static void sigusr1_handler(int sig) {
    LOG_ERROR("Caught signal %d (Watchdog Hang Detection)", sig);
    pc_print_backtrace();
    LOG_ERROR("Terminating due to hang...");
    exit(1);
}
#endif

void pc_debug_init(void) {
#ifndef _WIN32
    signal(SIGUSR1, sigusr1_handler);
#endif
}
