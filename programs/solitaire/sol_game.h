#ifndef SOL_GAME_H
#define SOL_GAME_H

#include "solitaire.h"

typedef struct {
    CARD  stock[52];
    int   stock_top;
    
    CARD  waste[52];
    int   waste_top;
    
    CARD  foundation[4][52];
    int   found_top[4];
    
    CARD  tableau[7][52];
    int   tab_top[7];
    
    int   score;
    DWORD start_tick;
} GameState;

extern GameState g_Game;

void Game_Init(void);
BOOL Game_CanDropTab(CARD c, int col);
BOOL Game_CanDropFound(CARD c, int f);

#endif