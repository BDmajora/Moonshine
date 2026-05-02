#ifndef SOL_ENDGAME_H
#define SOL_ENDGAME_H

#include "solitaire.h"

#define ENDGAME_TIMER_ID   2
#define ENDGAME_TIMER_MS   8
#define GRAVITY            0.5f
#define BOUNCE_DAMPEN      0.85f
#define NUM_BOUNCE_CARDS   52

typedef struct {
    CARD  card;
    float x, y;
    float vx, vy;
    BOOL  active;
} BounceCard;

extern BOOL g_endgame_active;

void EndGame_Start(HWND hwnd);
void EndGame_Stop(HWND hwnd);
void EndGame_Tick(HWND hwnd);
void EndGame_Draw(HDC hdc, int width, int height);
void EndGame_Dismiss(HWND hwnd);
BOOL EndGame_CheckWin(void);

#endif