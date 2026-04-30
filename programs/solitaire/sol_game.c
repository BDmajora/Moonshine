#include "sol_game.h"
#include <windows.h>

GameState g_Game;

/* ---- Internal Card Helpers ---- */
static int card_suit(CARD c)  { return c & 3; }
static int card_face(CARD c)  { return c >> 2; }
static BOOL is_red(CARD c)    { 
    int s = card_suit(c); 
    return s == CARD_SUIT_DIAMONDS || s == CARD_SUIT_HEARTS; 
}

void Game_Init(void) {
    int i, col, row;
    CARD deck[52];
    
    /* 1. Generate a standard deck */
    for(i = 0; i < 52; i++) {
        deck[i] = (CARD)i;
    }
    
    /* 2. Shuffle */
    Card_Shuffle(deck, 52); /* Assuming this is declared in sol_cards.h or solitaire.h */
    
    /* 3. Reset State */
    g_Game.stock_top = 0;
    g_Game.waste_top = 0;
    for(i = 0; i < 4; i++) g_Game.found_top[i] = 0;
    for(i = 0; i < 7; i++) g_Game.tab_top[i] = 0;
    g_Game.score = 0;
    g_Game.start_tick = GetTickCount();
    
    /* 4. Deal to Tableau */
    i = 0;
    for(col = 0; col < 7; col++) {
        g_Game.tab_top[col] = col + 1;
        for(row = 0; row <= col; row++) {
            /* Deal face down, except for the last card in the column */
            g_Game.tableau[col][row] = deck[i] | (row == col ? CARD_FACEUP : 0);
            i++;
        }
    }

    /* 5. Rest to stock, face down */
    while(i < 52) {
        g_Game.stock[g_Game.stock_top++] = deck[i++];
    }
}

BOOL Game_CanDropTab(CARD c, int col) {
    CARD top;
    
    if(!(c & CARD_FACEUP)) return FALSE;
    
    /* King on an empty column */
    if(g_Game.tab_top[col] == 0) return card_face(c) == 12; 
    
    top = g_Game.tableau[col][g_Game.tab_top[col] - 1];
    
    if(!(top & CARD_FACEUP)) return FALSE;
    
    /* Rules: Must be alternating color and descending face value */
    return card_face(top) == card_face(c) + 1 && is_red(top) != is_red(c);
}

BOOL Game_CanDropFound(CARD c, int f) {
    CARD top;
    
    if(!(c & CARD_FACEUP)) return FALSE;
    
    /* Ace on an empty foundation */
    if(g_Game.found_top[f] == 0) return card_face(c) == 0; 
    
    top = g_Game.foundation[f][g_Game.found_top[f] - 1];
    
    /* Rules: Must be same suit and ascending face value */
    return card_suit(c) == card_suit(top) && card_face(c) == card_face(top) + 1;
}