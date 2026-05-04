#ifndef SOL_GAME_H
#define SOL_GAME_H

#include "solitaire.h"

// Game state
typedef struct {
    CARD  stock[52];    // Draw pile
    int   stock_top;
    
    CARD  waste[52];    // Discard pile
    int   waste_top;
    
    CARD  foundation[4][52]; // Suit piles
    int   found_top[4];
    
    CARD  tableau[7][52];    // Play columns
    int   tab_top[7];
    
    int   score;        // Game score
    DWORD start_tick;   // Start time
} GameState;

extern GameState g_Game;

// Lifecycle and rules
void Game_Init(void);
BOOL Game_CanDropTab(CARD c, int col);
BOOL Game_CanDropFound(CARD c, int f);

#endif /* SOL_GAME_H */