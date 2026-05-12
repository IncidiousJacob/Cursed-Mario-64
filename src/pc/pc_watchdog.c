#include <SDL2/SDL.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "pc_watchdog.h"
#include "pc_log.h"

#define HANG_THRESHOLD_SECONDS 5

static volatile uint32_t s_heartbeat = 0;
static pthread_t s_main_thread;
static bool s_running = true;

static int watchdog_thread_func(void *data) {
    uint32_t last_heartbeat = 0;
    int seconds_since_last_change = 0;

    LOG_INFO("Watchdog thread started.");

    while (s_running) {
        SDL_Delay(1000); // Check every second

        if (s_heartbeat == last_heartbeat) {
            seconds_since_last_change++;
            if (seconds_since_last_change >= HANG_THRESHOLD_SECONDS) {
                LOG_ERROR("HANG DETECTED! No heartbeat for %d seconds.", seconds_since_last_change);
                // Send signal to main thread to trigger backtrace
                pthread_kill(s_main_thread, SIGUSR1);
                
                // If signal didn't terminate, we should probably exit anyway after some time
                SDL_Delay(2000);
                LOG_ERROR("Watchdog forcing exit after hang.");
                exit(1);
            }
        } else {
            last_heartbeat = s_heartbeat;
            seconds_since_last_change = 0;
        }
    }

    return 0;
}

void pc_watchdog_init(void) {
    s_main_thread = pthread_self();
    s_heartbeat = 0;
    SDL_CreateThread(watchdog_thread_func, "SM64Watchdog", NULL);
}

void pc_watchdog_heartbeat(void) {
    s_heartbeat++;
}
