#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include "solitaire.h"

/* XP Dimensions & Spacing */
#define X_MARGIN        12
#define Y_MARGIN        12
#define X_SPACING       82   
#define TAB_Y_TOP       110  
#define STACK_FACE_DOWN 3    
#define STACK_FACE_UP   15   

#define ID_GHOST_CARD   52   
#define ID_CARD_BACK    54   

CARD_STACK tableau[7][20]; 
int tableau_count[7];

int foundation_count[4] = {0, 0, 0, 0};
int stock_count = 24;
int waste_count = 0;

void InitGame(void) {
    int i, j;
    srand((unsigned int)time(NULL));
    
    for (i = 0; i < 7; i++) {
        tableau_count[i] = i + 1;
        for (j = 0; j <= i; j++) {
            tableau[i][j].card_id = rand() % 52;
            tableau[i][j].face_up = (j == i); 
        }
    }
}

void DrawBoard(HDC hdc, int width, int height) {
    /* FIXED: All variables declared at the start of the function for C90 */
    int i, j, x, current_y;

    /* --- 1. TOP ROW: STOCK & WASTE --- */
    if (stock_count > 0)
        cdtDraw(hdc, X_MARGIN, Y_MARGIN, ID_CARD_BACK, MODE_FACEDOWN, SOL_BG_COLOR);
    else
        cdtDraw(hdc, X_MARGIN, Y_MARGIN, ID_GHOST_CARD, MODE_FACEUP, SOL_BG_COLOR);

    if (waste_count > 0) {
        cdtDraw(hdc, X_MARGIN + X_SPACING, Y_MARGIN, 0, MODE_FACEUP, SOL_BG_COLOR);
    }

    /* --- 2. TOP ROW: FOUNDATIONS --- */
    for (i = 0; i < 4; i++) {
        x = X_MARGIN + ((3 + i) * X_SPACING);
        
        if (foundation_count[i] == 0)
            cdtDraw(hdc, x, Y_MARGIN, ID_GHOST_CARD, MODE_FACEUP, SOL_BG_COLOR);
        else
            cdtDraw(hdc, x, Y_MARGIN, rand() % 52, MODE_FACEUP, SOL_BG_COLOR);
    }

    /* --- 3. TABLEAU: THE 7 COLUMNS --- */
    for (i = 0; i < 7; i++) {
        x = X_MARGIN + (i * X_SPACING);
        
        if (tableau_count[i] == 0) {
            cdtDraw(hdc, x, TAB_Y_TOP, ID_GHOST_CARD, MODE_FACEUP, SOL_BG_COLOR);
            continue;
        }

        current_y = TAB_Y_TOP;
        for (j = 0; j < tableau_count[i]; j++) {
            if (tableau[i][j].face_up) {
                cdtDraw(hdc, x, current_y, tableau[i][j].card_id, MODE_FACEUP, SOL_BG_COLOR);
                current_y += STACK_FACE_UP;
            } else {
                cdtDraw(hdc, x, current_y, ID_CARD_BACK, MODE_FACEDOWN, SOL_BG_COLOR);
                current_y += STACK_FACE_DOWN;
            }
        }
    }
}