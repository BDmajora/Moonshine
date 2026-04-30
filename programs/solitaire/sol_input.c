#include <windows.h>
#include "solitaire.h"
#include "sol_input.h"

void OnMouseMove(HWND hwnd, int mx, int my) {
    if (!dragging) return;
    drag_mouse_x = mx;
    drag_mouse_y = my;
    InvalidateRect(hwnd, NULL, FALSE);
}

void OnLButtonDown(HWND hwnd, int mx, int my) {
    int i, col, row;

    if (dragging) return;

    /* 1. Check Stock (Restore Draw 3) */
    if (mx >= X_MARGIN && mx < X_MARGIN + 71 && my >= Y_MARGIN && my < Y_MARGIN + 96) {
        if (g_Game.stock_top > 0) {
            /* Flip up to 3 cards at once into the waste pile */
            int num_to_draw = (g_Game.stock_top >= 3) ? 3 : g_Game.stock_top;
            for (int k = 0; k < num_to_draw; k++) {
                g_Game.waste[g_Game.waste_top++] = g_Game.stock[--g_Game.stock_top] | CARD_FACEUP;
            }
        } else {
            /* Redeal logic remains the same */
            while (g_Game.waste_top > 0) {
                g_Game.stock[g_Game.stock_top++] = g_Game.waste[--g_Game.waste_top] & ~CARD_FACEUP;
            }
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    /* 2. Check Waste (Pick up top card) */
    if (g_Game.waste_top > 0) {
        int show = g_Game.waste_top < 3 ? g_Game.waste_top : 3;
        int wx = X_MARGIN + X_SPACING + ((show - 1) * WASTE_FAN_OFF);
        int wy = Y_MARGIN + (show - 1);

        if (mx >= wx && mx < wx + 71 && my >= wy && my < wy + 96) {
            dragging = TRUE;
            drag_from_type = SRC_WASTE;
            drag_count = 1;
            drag_cards[0] = g_Game.waste[g_Game.waste_top - 1];
            
            drag_mouse_x = mx;
            drag_mouse_y = my;
            drag_x_off = mx - wx;
            drag_y_off = my - wy;
            
            SetCapture(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }
    }

    /* 3. Check Tableau (Pick up a stack of face-up cards) */
    for (col = 0; col < 7; col++) {
        int cx = Layout_GetTabX(col);
        int top = g_Game.tab_top[col];

        if (top > 0) {
            for (row = top - 1; row >= 0; row--) {
                int cy = Layout_GetTabCardY(col, row);
                CARD c = g_Game.tableau[col][row];
                int height = (row == top - 1) ? 96 : ((c & CARD_FACEUP) ? FACE_UP_OFF : FACE_DOWN_OFF);

                if (mx >= cx && mx < cx + 71 && my >= cy && my < cy + height) {
                    if (c & CARD_FACEUP) {
                        dragging = TRUE;
                        drag_from_type = SRC_TAB;
                        drag_from_idx = col;
                        drag_count = top - row; /* The count is correctly calculated here */
                        
                        for (i = 0; i < drag_count; i++) {
                            drag_cards[i] = g_Game.tableau[col][row + i];
                        }
                        
                        drag_mouse_x = mx;
                        drag_mouse_y = my;
                        drag_x_off = mx - cx;
                        drag_y_off = my - cy;
                        
                        SetCapture(hwnd);
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    return;
                }
            }
        }
    }
}

void OnLButtonUp(HWND hwnd, int mx, int my) {
    int i, col;
    BOOL dropped = FALSE;
    CARD top_card;

    if (!dragging) return;

    top_card = drag_cards[0];
    dragging = FALSE;
    ReleaseCapture();

    /* Try Foundation check... */
    for (i = 0; i < 4; i++) {
        int fx = X_MARGIN + (3 + i) * X_SPACING;
        if (drag_count == 1 && Layout_CheckHit(mx, my, fx, Y_MARGIN)
                && Game_CanDropFound(top_card, i)) {
            g_Game.foundation[i][g_Game.found_top[i]++] = top_card;
            if (drag_from_type == SRC_WASTE)    g_Game.waste_top--;
            else if (drag_from_type == SRC_TAB) g_Game.tab_top[drag_from_idx]--;
            g_Game.score += 10;
            dropped = TRUE;
            break;
        }
    }

    /* Try Tableau (Restoring stack removal) */
    if (!dropped) {
        for (col = 0; col < 7; col++) {
            int cx = Layout_GetTabX(col);
            int n  = g_Game.tab_top[col];
            int ty = (n == 0) ? Y_TABLEAU : Layout_GetTabCardY(col, n - 1);

            if (mx >= cx && mx < cx + 71 && my >= ty && my < ty + 96) {
                if (Game_CanDropTab(top_card, col)) {
                    for (i = 0; i < drag_count; i++)
                        g_Game.tableau[col][g_Game.tab_top[col]++] = drag_cards[i];

                    if (drag_from_type == SRC_WASTE) {
                        g_Game.waste_top--;
                        g_Game.score += 5;
                    } else if (drag_from_type == SRC_TAB) {
                        /* FIX: Subtract the full count, not just -- */
                        g_Game.tab_top[drag_from_idx] -= drag_count; 
                    }
                    dropped = TRUE;
                    break;
                }
            }
        }
    }

    /* Auto-flip logic remains the same... */
    if (dropped && drag_from_type == SRC_TAB) {
        int old_col = drag_from_idx;
        int top = g_Game.tab_top[old_col];
        if (top > 0 && !(g_Game.tableau[old_col][top - 1] & CARD_FACEUP)) {
            g_Game.tableau[old_col][top - 1] |= CARD_FACEUP;
            g_Game.score += 5;
        }
    }

    InvalidateRect(hwnd, NULL, FALSE);
}