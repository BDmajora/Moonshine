#include "solitaire.h"
#include "sol_layout.h"

// Returns the vertical spacing based on card orientation
static int GetCardOffset(int card) {
    return (card & CARD_FACEUP) ? FACE_UP_OFF : FACE_DOWN_OFF;
}

// Calculates horizontal position of a tableau column
int Layout_GetTabX(int col) {
    return X_MARGIN + (col * X_SPACING);
}

// Calculates vertical position of a specific card in a column
int Layout_GetTabCardY(int col, int row) {
    int y = Y_TABLEAU;
    for (int i = 0; i < row; i++) {
        y += GetCardOffset(g_Game.tableau[col][i]);
    }
    return y;
}

// Returns card index at mouse coordinates or -1 if empty
int Layout_HitTabCard(int col, int mx, int my) {
    int x = Layout_GetTabX(col);
    int top = g_Game.tab_top[col];
    int row, y, hit_h;

    if (top == 0) {
        return Layout_CheckHit(mx, my, x, Y_TABLEAU) ? 0 : -1;
    }

    // Check cards top-to-bottom for selection priority
    for (row = top - 1; row >= 0; row--) {
        y = Layout_GetTabCardY(col, row);
        hit_h = (row == top - 1) ? CARD_HEIGHT : GetCardOffset(g_Game.tableau[col][row]);

        if (mx >= x && mx < x + CARD_WIDTH && my >= y && my < y + hit_h) 
            return row;
    }
    return -1;
}

// Checks if coordinates fall within a standard card rectangle
BOOL Layout_CheckHit(int mx, int my, int cx, int cy) {
    return (mx >= cx && mx < cx + CARD_WIDTH && my >= cy && my < cy + CARD_HEIGHT);
}