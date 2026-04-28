#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include "solitaire.h"

/* Simplified representation of the 7 tableau piles */
CARD_STACK tableau[7][20]; 
int tableau_count[7];

void InitGame(void) {
    int i, j;
    srand((unsigned int)time(NULL));
    
    /* Reset and "Deal" logic */
    for (i = 0; i < 7; i++) {
        tableau_count[i] = i + 1;
        for (j = 0; j <= i; j++) {
            /* Random card for demo purposes */
            tableau[i][j].card_id = rand() % 52;
            tableau[i][j].face_up = (j == i);
        }
    }
}

void DrawBoard(HDC hdc, int width, int height) {
    int i, j;
    int margin_x = 20;
    int margin_y = 20;
    int spacing_x = (width - 40) / 7;
    int stack_offset = 15;

    /* Draw the Tableau (the 7 columns) */
    for (i = 0; i < 7; i++) {
        for (j = 0; j < tableau_count[i]; j++) {
            int x = margin_x + (i * spacing_x);
            int y = margin_y + 120 + (j * stack_offset);
            
            if (tableau[i][j].face_up)
                cdtDraw(hdc, x, y, tableau[i][j].card_id, MODE_FACEUP, SOL_BG_COLOR);
            else
                cdtDraw(hdc, x, y, CARD_BACK_WEAVE1, MODE_FACEDOWN, SOL_BG_COLOR);
        }
    }

    /* Draw the Stock Pile placeholder */
    cdtDraw(hdc, margin_x, margin_y, CARD_BACK_WEAVE1, MODE_FACEDOWN, SOL_BG_COLOR);
}