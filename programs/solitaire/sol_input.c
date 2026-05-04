#include "solitaire.h"
#include "sol_input.h"
#include "sol_endgame.h"
#include "sol_timer.h"
#include "sol_layout.h" // Essential for CARD_WIDTH, CARD_HEIGHT, and Hit helpers

/* ==========================================================================
   SECTION 0: Encapsulated State
   ========================================================================== */

/* This file owns the drag state. No one else can modify it directly. */
static DragState g_Drag = {0};

const DragState* Input_GetDragInfo(void) {
    return &g_Drag;
}

/* ==========================================================================
   SECTION 1: High-Level Dispatchers
   ========================================================================== */

void Input_OnMouse(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    short mx = (short)LOWORD(lp);
    short my = (short)HIWORD(lp);

    if (g_endgame_active) {
        if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN) {
            EndGame_Dismiss(hwnd);
        } else if (msg == WM_MOUSEMOVE) {
            EndGame_MouseMove(mx, my);
        }
        return;
    }

    switch (msg) {
        case WM_LBUTTONDOWN: OnLButtonDown(hwnd, mx, my); break;
        case WM_LBUTTONUP:   OnLButtonUp(hwnd, mx, my);   break;
        case WM_MOUSEMOVE:   OnMouseMove(hwnd, mx, my);   break;
    }
}

BOOL Input_OnKeyboard(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SYSKEYDOWN && wp == '2') {
        if ((GetKeyState(VK_MENU) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) {
            Input_CheatWin(hwnd);
            return TRUE;
        }
    }

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
    if (!g_Drag.is_dragging) return;
    g_Drag.mouse_x = mx;
    g_Drag.mouse_y = my;
    InvalidateRect(hwnd, NULL, FALSE);
}

void OnLButtonDown(HWND hwnd, int mx, int my) {
    int i, col, row;

    Timer_Start(); 

    if (g_Drag.is_dragging) return;

    /* 1. Check Stock (Updated to use constants) */
    if (mx >= X_MARGIN && mx < X_MARGIN + CARD_WIDTH && 
        my >= Y_MARGIN && my < Y_MARGIN + CARD_HEIGHT) {
        
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

    /* 2. Check Waste (Updated to use constants) */
    if (g_Game.waste_top > 0) {
        int show = g_Game.waste_top < 3 ? g_Game.waste_top : 3;
        int wx = X_MARGIN + X_SPACING + ((show - 1) * WASTE_FAN_OFF);
        int wy = Y_MARGIN + (show - 1); // Logic for the slight Y offset in drawing

        if (mx >= wx && mx < wx + CARD_WIDTH && my >= wy && my < wy + CARD_HEIGHT) {
            g_Drag.is_dragging = TRUE;
            g_Drag.from_type = SRC_WASTE;
            g_Drag.count = 1;
            g_Drag.cards[0] = g_Game.waste[g_Game.waste_top - 1];
            
            g_Drag.mouse_x = mx;
            g_Drag.mouse_y = my;
            g_Drag.x_off = mx - wx;
            g_Drag.y_off = my - wy;
            
            SetCapture(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }
    }

    /* 3. Check Tableau (Using updated layout logic) */
    for (col = 0; col < 7; col++) {
        row = Layout_HitTabCard(col, mx, my); 
        
        if (row != -1) {
            CARD c = g_Game.tableau[col][row];
            
            // Only allow dragging if the card is face up
            if (c & CARD_FACEUP) {
                int cx = Layout_GetTabX(col);
                int cy = Layout_GetTabCardY(col, row);

                g_Drag.is_dragging = TRUE;
                g_Drag.from_type = SRC_TAB;
                g_Drag.from_idx = col;
                g_Drag.count = g_Game.tab_top[col] - row;
                
                for (i = 0; i < g_Drag.count; i++) {
                    g_Drag.cards[i] = g_Game.tableau[col][row + i];
                }
                
                g_Drag.mouse_x = mx;
                g_Drag.mouse_y = my;
                g_Drag.x_off = mx - cx;
                g_Drag.y_off = my - cy;
                
                SetCapture(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return;
        }
    }
}

void OnLButtonUp(HWND hwnd, int mx, int my) {
    int i, col;
    BOOL dropped = FALSE;
    CARD top_card;

    if (!g_Drag.is_dragging) return;

    top_card = g_Drag.cards[0];
    g_Drag.is_dragging = FALSE;
    ReleaseCapture();

    /* Try Foundation drop */
    for (i = 0; i < 4; i++) {
        int fx = X_MARGIN + (3 + i) * X_SPACING;
        // Using Layout_CheckHit to verify the drop zone
        if (g_Drag.count == 1 && 
            mx >= fx && mx < fx + CARD_WIDTH && 
            my >= Y_MARGIN && my < Y_MARGIN + CARD_HEIGHT &&
            Game_CanDropFound(top_card, i)) {
            
            g_Game.foundation[i][g_Game.found_top[i]++] = top_card;
            
            if (g_Drag.from_type == SRC_WASTE)    g_Game.waste_top--;
            else if (g_Drag.from_type == SRC_TAB) g_Game.tab_top[g_Drag.from_idx]--;
            
            g_Game.score += 10;
            dropped = TRUE;

            if (g_Game.found_top[0] + g_Game.found_top[1] + 
                g_Game.found_top[2] + g_Game.found_top[3] == 52) {
                KillTimer(hwnd, 1);
                EndGame_Start(hwnd);
            }
            break;
        }
    }

    /* Try Tableau drop */
    if (!dropped) {
        for (col = 0; col < 7; col++) {
            int cx = Layout_GetTabX(col);
            int n  = g_Game.tab_top[col];
            // If empty, target is the Y_TABLEAU base; if full, target is the last card
            int ty = (n == 0) ? Y_TABLEAU : Layout_GetTabCardY(col, n - 1);

            if (mx >= cx && mx < cx + CARD_WIDTH && my >= ty && my < ty + CARD_HEIGHT) {
                if (Game_CanDropTab(top_card, col)) {
                    for (i = 0; i < g_Drag.count; i++)
                        g_Game.tableau[col][g_Game.tab_top[col]++] = g_Drag.cards[i];

                    if (g_Drag.from_type == SRC_WASTE) {
                        g_Game.waste_top--;
                        g_Game.score += 5;
                    } else if (g_Drag.from_type == SRC_TAB) {
                        g_Game.tab_top[g_Drag.from_idx] -= g_Drag.count; 
                    }
                    dropped = TRUE;
                    break;
                }
            }
        }
    }

    /* Auto-flip logic */
    if (dropped && g_Drag.from_type == SRC_TAB) {
        int old_col = g_Drag.from_idx;
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