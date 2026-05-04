#include "sol_game.h"
#include "sol_cards.h"
#include "sol_score.h"
#include <windows.h>

GameState g_Game;

// Initialize game state and deal
void Game_Init(void) {
    int i, col, row;
    CARD deck[52];
    
    // Generate and shuffle deck
    Card_GenerateDeck(deck);
    Card_Shuffle(deck, 52); 
    
    // Reset all piles
    g_Game.stock_top = 0;
    g_Game.waste_top = 0;
    for(i = 0; i < 4; i++) g_Game.found_top[i] = 0;
    for(i = 0; i < 7; i++) g_Game.tab_top[i] = 0;
    
    Score_Init(); 
    g_Game.start_tick = GetTickCount();
    
    // Deal to tableau
    i = 0;
    for(col = 0; col < 7; col++) {
        g_Game.tab_top[col] = col + 1;
        for(row = 0; row <= col; row++) {
            // Last card in column is face up
            g_Game.tableau[col][row] = deck[i] | (row == col ? CARD_FACEUP : 0);
            i++;
        }
    }

    // Move remainder to stock
    while(i < 52) {
        g_Game.stock[g_Game.stock_top++] = deck[i++];
    }
}

// Validate tableau placement
BOOL Game_CanDropTab(CARD c, int col) {
    CARD top;
    
    if(!(c & CARD_FACEUP)) return FALSE;
    
    // King on empty column
    if(g_Game.tab_top[col] == 0) return Card_Face(c) == 12; 
    
    top = g_Game.tableau[col][g_Game.tab_top[col] - 1];
    
    if(!(top & CARD_FACEUP)) return FALSE;
    
    // Alt color, descending rank
    return Card_Face(top) == Card_Face(c) + 1 && Card_IsRed(top) != Card_IsRed(c);
}

// Validate foundation placement
BOOL Game_CanDropFound(CARD c, int f) {
    CARD top;
    
    if(!(c & CARD_FACEUP)) return FALSE;
    
    // Ace on empty foundation
    if(g_Game.found_top[f] == 0) return Card_Face(c) == 0; 
    
    top = g_Game.foundation[f][g_Game.found_top[f] - 1];
    
    // Same suit, ascending rank
    return Card_Suit(c) == Card_Suit(top) && Card_Face(c) == Card_Face(top) + 1;
}