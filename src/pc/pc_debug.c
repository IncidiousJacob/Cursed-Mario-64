#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
static HANDLE s_main_thread_handle;
#else
#include <execinfo.h>
#include <pthread.h>
static pthread_t s_main_thread;
#endif

#include "pc_debug.h"
#include "pc_log.h"

void pc_print_backtrace(void) {
#ifdef _WIN32
    void *stack[32];
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(process, NULL, TRUE);

    unsigned short frames = CaptureStackBackTrace(0, 32, stack, NULL);
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)calloc(1, sizeof(SYMBOL_INFO) + 256 * sizeof(char));
    symbol->MaxNameLen = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    LOG_ERROR("Backtrace (%d frames):", frames);
    for (unsigned short i = 0; i < frames; i++) {
        DWORD64 displacement = 0;
        IMAGEHLP_MODULE64 module;
        memset(&module, 0, sizeof(IMAGEHLP_MODULE64));
        module.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
        
        const char *module_name = "???";
        if (SymGetModuleInfo64(process, (DWORD64)stack[i], &module)) {
            module_name = module.ModuleName;
        }

        if (SymFromAddr(process, (DWORD64)(stack[i]), &displacement, symbol)) {
            LOG_ERROR("  %d: %s!%s - 0x%0llX", i, module_name, symbol->Name, (unsigned long long)symbol->Address);
        } else {
            LOG_ERROR("  %d: %s + 0x%0llX", i, module_name, (unsigned long long)((DWORD64)stack[i] - module.BaseOfImage));
        }
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

void pc_print_main_thread_backtrace(void) {
#ifdef _WIN32
    if (!s_main_thread_handle) return;
    
    LOG_ERROR("Attempting to capture main thread backtrace...");
    
    CONTEXT context;
    memset(&context, 0, sizeof(CONTEXT));
    context.ContextFlags = CONTEXT_FULL;
    
    if (SuspendThread(s_main_thread_handle) != (DWORD)-1) {
        if (GetThreadContext(s_main_thread_handle, &context)) {
            STACKFRAME64 stackframe;
            memset(&stackframe, 0, sizeof(STACKFRAME64));
            
            HANDLE process = GetCurrentProcess();
            DWORD machine_type;
            
#ifdef _M_IX86
            machine_type = IMAGE_FILE_MACHINE_I386;
            stackframe.AddrPC.Offset = context.Eip;
            stackframe.AddrPC.Mode = AddrModeFlat;
            stackframe.AddrFrame.Offset = context.Ebp;
            stackframe.AddrFrame.Mode = AddrModeFlat;
            stackframe.AddrStack.Offset = context.Esp;
            stackframe.AddrStack.Mode = AddrModeFlat;
#elif defined(_M_X64) || defined(__x86_64__)
            machine_type = IMAGE_FILE_MACHINE_AMD64;
            stackframe.AddrPC.Offset = context.Rip;
            stackframe.AddrPC.Mode = AddrModeFlat;
            stackframe.AddrFrame.Offset = context.Rbp;
            stackframe.AddrFrame.Mode = AddrModeFlat;
            stackframe.AddrStack.Offset = context.Rsp;
            stackframe.AddrStack.Mode = AddrModeFlat;
#else
            LOG_ERROR("Unsupported architecture for StackWalk64");
            ResumeThread(s_main_thread_handle);
            return;
#endif

            SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
            SymInitialize(process, NULL, TRUE);
            
            LOG_ERROR("Main Thread Backtrace:");
            for (int i = 0; i < 32; i++) {
                if (!StackWalk64(machine_type, process, s_main_thread_handle, &stackframe, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
                    break;
                
                DWORD64 displacement = 0;
                char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
                SYMBOL_INFO *symbol = (SYMBOL_INFO *)buffer;
                symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                symbol->MaxNameLen = MAX_SYM_NAME;
                
                IMAGEHLP_MODULE64 module;
                memset(&module, 0, sizeof(IMAGEHLP_MODULE64));
                module.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
                
                const char *module_name = "???";
                if (SymGetModuleInfo64(process, stackframe.AddrPC.Offset, &module)) {
                    module_name = module.ModuleName;
                }

                if (SymFromAddr(process, stackframe.AddrPC.Offset, &displacement, symbol)) {
                    LOG_ERROR("  %d: %s!%s - 0x%0llX", i, module_name, symbol->Name, (unsigned long long)symbol->Address);
                } else {
                    LOG_ERROR("  %d: %s + 0x%0llX", i, module_name, (unsigned long long)(stackframe.AddrPC.Offset - module.BaseOfImage));
                }
            }
        }
        ResumeThread(s_main_thread_handle);
    } else {
        LOG_ERROR("Failed to suspend main thread.");
    }
#else
    pthread_kill(s_main_thread, SIGUSR1);
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
#ifdef _WIN32
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &s_main_thread_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
#else
    s_main_thread = pthread_self();
    signal(SIGUSR1, sigusr1_handler);
#endif
}
