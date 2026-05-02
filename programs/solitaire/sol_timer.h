#ifndef SOL_TIMER_H
#define SOL_TIMER_H

#include <windows.h>

/**
 * Timer_Start: Marks the beginning of the game. 
 * Should be called on the first valid mouse click.
 */
void  Timer_Start(void);

/**
 * Timer_Reset: Resets the clock to zero and stops it.
 * Should be called when a new game is dealt.
 */
void  Timer_Reset(void);

/**
 * Timer_GetElapsed: Returns the seconds passed since Timer_Start.
 * Returns 0 if the timer hasn't started yet.
 */
DWORD Timer_GetElapsed(void);

/**
 * Timer_IsRunning: Checks if the game clock is active.
 */
BOOL  Timer_IsRunning(void);

#endif