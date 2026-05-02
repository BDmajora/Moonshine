#include "sol_timer.h"

static DWORD s_start_tick = 0;
static BOOL  s_running    = FALSE;

void Timer_Start(void) {
    if (!s_running) {
        s_start_tick = GetTickCount();
        s_running = TRUE;
    }
}

void Timer_Reset(void) {
    s_start_tick = 0;
    s_running = FALSE;
}

BOOL Timer_IsRunning(void) {
    return s_running;
}

DWORD Timer_GetElapsed(void) {
    if (!s_running) {
        return 0;
    }
    return (GetTickCount() - s_start_tick) / 1000;
}