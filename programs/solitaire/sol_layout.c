#include "solitaire.h"
#include "sol_layout.h"

int Layout_GetTabX(int col) {
    return X_MARGIN + (col * X_SPACING);
}

int Layout_GetTabCardY(int col, int row) {
    int y = Y_TABLEAU;
    int i;
    
    /* Calculate dynamic Y offset based on the cards below this one */
    for (i = 0; i < row; i++) {
        if (g_Game.tableau[col][i] & CARD_FACEUP) {
            y += FACE_UP_OFF;
        } else {
            y += FACE_DOWN_OFF;
        }
    }
    return y;
}

int Layout_HitTabCard(int col, int mx, int my) {
    int x = Layout_GetTabX(col);
    int top = g_Game.tab_top[col];
    int row, y;
    
    if (top == 0) {
        if (Layout_CheckHit(mx, my, x, Y_TABLEAU)) return 0;
        return -1;
    }

    /* Check cards from top to bottom (reverse order) for click priority */
    for (row = top - 1; row >= 0; row--) {
        y = Layout_GetTabCardY(col, row);
        
        /* If it's the top card, the hit area is the full card height.
           If it's covered, the hit area is only the visible offset. */
        int hit_h = (row == top - 1) ? CARD_HEIGHT : 
                    ((g_Game.tableau[col][row] & CARD_FACEUP) ? FACE_UP_OFF : FACE_DOWN_OFF);

        if (mx >= x && mx < x + CARD_WIDTH && my >= y && my < y + hit_h) 
            return row;
    }
    return -1;
}

BOOL Layout_CheckHit(int mx, int my, int cx, int cy) {
    return (mx >= cx && mx < cx + CARD_WIDTH && my >= cy && my < cy + CARD_HEIGHT);
}