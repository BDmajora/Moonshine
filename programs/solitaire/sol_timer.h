#ifndef SOL_TIMER_H
#define SOL_TIMER_H

#include <windows.h>

// Starts the timer on first interaction
void  Timer_Start(void);

// Stops and resets the timer for a new game
void  Timer_Reset(void);

// Returns seconds elapsed since timer started
DWORD Timer_GetElapsed(void);

// Returns true if the timer is currently active
BOOL  Timer_IsRunning(void);

#endif // SOL_TIMER_H