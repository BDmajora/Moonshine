#include "sol_timer.h"

static DWORD s_start_tick = 0;
static BOOL  s_running    = FALSE;

// Starts the game timer if not already running
void Timer_Start(void) {
    if (!s_running) {
        s_start_tick = GetTickCount();
        s_running = TRUE;
    }
}

// Stops and resets the timer to zero
void Timer_Reset(void) {
    s_start_tick = 0;
    s_running = FALSE;
}

// Returns the current active state of the timer
BOOL Timer_IsRunning(void) {
    return s_running;
}

// Returns elapsed time in seconds, or 0 if stopped
DWORD Timer_GetElapsed(void) {
    if (!s_running) {
        return 0;
    }
    return (GetTickCount() - s_start_tick) / 1000;
}