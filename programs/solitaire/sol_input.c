#include "solitaire.h"
#include "sol_input.h"
#include "sol_endgame.h"
#include "sol_timer.h" // Handles Timer_Start()

/* ==========================================================================
   SECTION 1: High-Level Dispatchers
   ========================================================================== */

void Input_OnMouse(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    short mx = (short)LOWORD(lp);
    short my = (short)HIWORD(lp);

    /* 1. Global EndGame Override */
    if (g_endgame_active) {
        if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN) {
            EndGame_Dismiss(hwnd);
        } else if (msg == WM_MOUSEMOVE) {
            EndGame_MouseMove(mx, my);
        }
        return;
    }

    /* 2. Standard Game Input */
    switch (msg) {
        case WM_LBUTTONDOWN: OnLButtonDown(hwnd, mx, my); break;
        case WM_LBUTTONUP:   OnLButtonUp(hwnd, mx, my);   break;
        case WM_MOUSEMOVE:   OnMouseMove(hwnd, mx, my);   break;
    }
}

BOOL Input_OnKeyboard(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    /* Handle Cheat: Alt + Shift + 2 */
    if (msg == WM_SYSKEYDOWN && wp == '2') {
        if ((GetKeyState(VK_MENU) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) {
            Input_CheatWin(hwnd);
            return TRUE;
        }
    }

    /* Handle Escape during EndGame */
    if (g_endgame_active && msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        EndGame_Dismiss(hwnd);
        return TRUE;
    }

    return FALSE;
}

void Input_OnCommand(HWND hwnd, int menuId) {
    switch (menuId) {
        case IDM_GAME_EXIT:
            PostQuitMessage(0);
            break;
        case IDM_GAME_DEAL:
            EndGame_Stop(hwnd);
            Game_Init();
            InvalidateRect(hwnd, NULL, TRUE);
            break;
    }
}

void Input_OnTimer(HWND hwnd, WPARAM timerId) {
    if (timerId == ENDGAME_TIMER_ID) {
        EndGame_Tick(hwnd);
    } else {
        OnTimer(hwnd);
    }
}

/* ==========================================================================
   SECTION 2: Specific Game Logic Handlers
   ========================================================================== */

void OnMouseMove(HWND hwnd, int mx, int my) {
    if (!dragging) return;
    drag_mouse_x = mx;
    drag_mouse_y = my;
    InvalidateRect(hwnd, NULL, FALSE);
}

void OnLButtonDown(HWND hwnd, int mx, int my) {
    int i, col, row;

    Timer_Start(); // Starts the clock on the very first click

    if (dragging) return;

    /* 1. Check Stock (Restore Draw 3) */
    if (mx >= X_MARGIN && mx < X_MARGIN + 71 && my >= Y_MARGIN && my < Y_MARGIN + 96) {
        if (g_Game.stock_top > 0) {
            int num_to_draw = (g_Game.stock_top >= 3) ? 3 : g_Game.stock_top;
            for (int k = 0; k < num_to_draw; k++) {
                g_Game.waste[g_Game.waste_top++] = g_Game.stock[--g_Game.stock_top] | CARD_FACEUP;
            }
        } else {
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
                        drag_count = top - row;
                        
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

    /* Try Foundation drop */
    for (i = 0; i < 4; i++) {
        int fx = X_MARGIN + (3 + i) * X_SPACING;
        if (drag_count == 1 && Layout_CheckHit(mx, my, fx, Y_MARGIN)
                && Game_CanDropFound(top_card, i)) {
            
            g_Game.foundation[i][g_Game.found_top[i]++] = top_card;
            
            if (drag_from_type == SRC_WASTE)    g_Game.waste_top--;
            else if (drag_from_type == SRC_TAB) g_Game.tab_top[drag_from_idx]--;
            
            g_Game.score += 10;
            dropped = TRUE;

            /* WIN CHECK */
            if (g_Game.found_top[0] + g_Game.found_top[1] + 
                g_Game.found_top[2] + g_Game.found_top[3] == 52) {
                KillTimer(hwnd, 1);
                EndGame_Start(hwnd); // Trigger the card-jumping animation
            }
            break;
        }
    }

    /* Try Tableau drop */
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
                        g_Game.tab_top[drag_from_idx] -= drag_count; 
                    }
                    dropped = TRUE;
                    break;
                }
            }
        }
    }

    /* Auto-flip logic */
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

/* ==========================================================================
   SECTION 3: Utility / Cheats
   ========================================================================== */

void Input_CheatWin(HWND hwnd) {
    int s, f;
    g_Game.score = 0;

    for (s = 0; s < 4; s++) {
        g_Game.found_top[s] = 13;
        for (f = 0; f < 13; f++) {
            g_Game.foundation[s][f] = (CARD)((f * 4) + s) | CARD_FACEUP;
        }
    }
    EndGame_Start(hwnd);
}