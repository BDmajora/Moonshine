#ifndef SOL_ENDGAME_H
#define SOL_ENDGAME_H

#include "solitaire.h"

// Animation timers and physics limits
#define ENDGAME_TIMER_ID   2
#define ENDGAME_TIMER_MS   16      
#define GRAVITY            0.8f    
#define BOUNCE_DAMPEN      0.75f   
#define NUM_BOUNCE_CARDS   52

typedef struct {
    CARD  card;
    float x, y;
    float vx, vy;
    BOOL  active;
} BounceCard;

extern BOOL g_endgame_active;

// Animation lifecycle and control
void EndGame_Start(HWND hwnd);
void EndGame_StartCheat(HWND hwnd);
void EndGame_Stop(HWND hwnd);
void EndGame_Tick(HWND hwnd);
void EndGame_Draw(HDC hdc, int width, int height);
void EndGame_Dismiss(HWND hwnd);
BOOL EndGame_CheckWin(void);
void EndGame_MouseMove(int mx, int my);

#endif