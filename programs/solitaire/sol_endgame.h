#ifndef SOL_ENDGAME_H
#define SOL_ENDGAME_H

#include "solitaire.h"

#define ENDGAME_TIMER_ID   2
#define ENDGAME_TIMER_MS   16      /* 16ms gives a smoother ~60 FPS update */
#define GRAVITY            0.8f    /* Matches your 0.5 to 1.0 recommendation */
#define BOUNCE_DAMPEN      0.75f   /* Retains 75% of velocity (your -0.7 to -0.8 range) */
#define NUM_BOUNCE_CARDS   52

typedef struct {
    CARD  card;
    float x, y;
    float vx, vy;
    BOOL  active;
} BounceCard;

extern BOOL g_endgame_active;

void EndGame_Start(HWND hwnd);
void EndGame_StartCheat(HWND hwnd);
void EndGame_Stop(HWND hwnd);
void EndGame_Tick(HWND hwnd);
void EndGame_Draw(HDC hdc, int width, int height);
void EndGame_Dismiss(HWND hwnd);
BOOL EndGame_CheckWin(void);
void EndGame_MouseMove(int mx, int my);

#endif