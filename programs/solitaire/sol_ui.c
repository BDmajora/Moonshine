#include "solitaire.h"

#define STATUS_MARGIN      12
#define WASTE_FAN_OFF      12

/* Drag state */
static BOOL  dragging = FALSE;
static CARD  drag_cards[52];
static int   drag_count = 0;
static int   drag_from_type;
static int   drag_from_idx;
static int   drag_mouse_x, drag_mouse_y;
static int   drag_x_off, drag_y_off;

#define SRC_WASTE  1
#define SRC_TAB    2
#define SRC_FOUND  3

#define CARD_BACK_RED 54

void OnTimer(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    rc.top = rc.bottom - STATUS_BAR_HEIGHT;
    InvalidateRect(hwnd, &rc, FALSE);
}

void DrawBoard(HDC hdc, int width, int height) {
    int i, col, row, y;
    WCHAR buf[128];
    RECT rcStatus, rcText;
    HBRUSH hbr;
    HPEN hpen, hOldPen;
    HFONT hfont, hOldFont;
    DWORD elapsed;

    /* 1. Status Bar */
    rcStatus.left = 0;
    rcStatus.top = height - STATUS_BAR_HEIGHT;
    rcStatus.right = width;
    rcStatus.bottom = height;

    hbr = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rcStatus, hbr);
    DeleteObject(hbr);

    hpen = CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
    hOldPen = (HPEN)SelectObject(hdc, hpen);
    MoveToEx(hdc, 0, height - STATUS_BAR_HEIGHT, NULL);
    LineTo(hdc, width, height - STATUS_BAR_HEIGHT);
    SelectObject(hdc, hOldPen);
    DeleteObject(hpen);

    hfont = CreateFontW(16, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"MS Sans Serif");
    hOldFont = (HFONT)SelectObject(hdc, hfont);
    SetBkMode(hdc, TRANSPARENT);

    elapsed = (GetTickCount() - g_Game.start_tick) / 1000;
    wsprintfW(buf, L"Score: %d  Time: %d", g_Game.score, (int)elapsed);
    rcText = rcStatus;
    rcText.right -= STATUS_MARGIN;
    DrawTextW(hdc, buf, -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

    SelectObject(hdc, hOldFont);
    DeleteObject(hfont);

    /* 2. Stock - Restored to top-left fanning (Up and Left) */
    if (g_Game.stock_top > 0) {
        if (g_Game.stock_top > 5)
            cdtDraw(hdc, X_MARGIN - 4, Y_MARGIN - 4, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
        if (g_Game.stock_top > 1)
            cdtDraw(hdc, X_MARGIN - 2, Y_MARGIN - 2, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
        cdtDraw(hdc, X_MARGIN, Y_MARGIN, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
    } else {
        /* Show the re-deal circle (card 53) if there are cards in the waste */
        if (g_Game.waste_top > 0)
            cdtDraw(hdc, X_MARGIN, Y_MARGIN, 53, MODE_FACEUP, SOL_BG_COLOR);
        else
            cdtDraw(hdc, X_MARGIN, Y_MARGIN, 0, MODE_GHOST, SOL_BG_COLOR);
    }

    /* 3. Waste - Subtle Diagonal fanning (1-pixel vertical step) */
    if (g_Game.waste_top > 0) {
        int show = g_Game.waste_top < 3 ? g_Game.waste_top : 3;
        int wx_base = X_MARGIN + X_SPACING;
        
        for (i = 0; i < show; i++) {
            int wx = wx_base + (i * WASTE_FAN_OFF);
            int wy = Y_MARGIN + i; // Changed to a 1-pixel vertical step for subtlety
            
            if (dragging && drag_from_type == SRC_WASTE && i == show - 1)
                continue;

            cdtDraw(hdc, wx, wy,
                    g_Game.waste[g_Game.waste_top - show + i] & ~CARD_FACEUP,
                    MODE_FACEUP, SOL_BG_COLOR);
        }
    }

    /* 4. Foundations */
    for (i = 0; i < 4; i++) {
        int fx = X_MARGIN + (3 + i) * X_SPACING;
        if (g_Game.found_top[i] == 0)
            cdtDraw(hdc, fx, Y_MARGIN, 0, MODE_GHOST, SOL_BG_COLOR);
        else
            cdtDraw(hdc, fx, Y_MARGIN,
                    g_Game.foundation[i][g_Game.found_top[i] - 1] & ~CARD_FACEUP,
                    MODE_FACEUP, SOL_BG_COLOR);
    }

    /* 5. Tableau */
    for (col = 0; col < 7; col++) {
        int cx = Layout_GetTabX(col);
        if (g_Game.tab_top[col] == 0) {
            cdtDraw(hdc, cx, Y_TABLEAU, 0, MODE_GHOST, SOL_BG_COLOR);
            continue;
        }

        y = Y_TABLEAU;
        for (row = 0; row < g_Game.tab_top[col]; row++) {
            CARD c = g_Game.tableau[col][row];

            if (dragging && drag_from_type == SRC_TAB && drag_from_idx == col
                    && row >= g_Game.tab_top[col] - drag_count)
                break;

            if (c & CARD_FACEUP) {
                cdtDraw(hdc, cx, y, c & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
                y += FACE_UP_OFF;
            } else {
                cdtDraw(hdc, cx, y, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
                y += FACE_DOWN_OFF;
            }
        }
    }

    /* 6. Drag stack */
    if (dragging) {
        int dx = drag_mouse_x - drag_x_off;
        int dy = drag_mouse_y - drag_y_off;
        for (i = 0; i < drag_count; i++)
            cdtDraw(hdc, dx, dy + (i * FACE_UP_OFF),
                    drag_cards[i] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
    }
}

void OnLButtonDown(HWND hwnd, int mx, int my) {
    int col, row, i;

    /* Stock */
    if (Layout_CheckHit(mx, my, X_MARGIN, Y_MARGIN)) {
        if (g_Game.stock_top > 0) {
            int draw = g_Game.stock_top < 3 ? g_Game.stock_top : 3;
            for (i = 0; i < draw; i++)
                g_Game.waste[g_Game.waste_top++] = g_Game.stock[--g_Game.stock_top] | CARD_FACEUP;
        } else {
            while (g_Game.waste_top > 0)
                g_Game.stock[g_Game.stock_top++] = g_Game.waste[--g_Game.waste_top] & ~CARD_FACEUP;
            g_Game.score = (g_Game.score > 100) ? g_Game.score - 100 : 0;
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    /* Waste - hit test the top card with the 1-pixel vertical offset */
    if (g_Game.waste_top > 0) {
        int show = g_Game.waste_top < 3 ? g_Game.waste_top : 3;
        int wx = X_MARGIN + X_SPACING + (show - 1) * WASTE_FAN_OFF;
        int wy = Y_MARGIN + (show - 1); // Match the subtle 1-pixel step

        if (Layout_CheckHit(mx, my, wx, wy)) {
            drag_cards[0] = g_Game.waste[g_Game.waste_top - 1];
            drag_count = 1;
            drag_from_type = SRC_WASTE;
            drag_x_off = mx - wx;
            drag_y_off = my - wy;
            dragging = TRUE;
            drag_mouse_x = mx;
            drag_mouse_y = my;
            SetCapture(hwnd);
            return;
        }
    }

    /* Tableau */
    for (col = 0; col < 7; col++) {
        row = Layout_HitTabCard(col, mx, my);
        if (row < 0) continue;

        if (!(g_Game.tableau[col][row] & CARD_FACEUP) && row == g_Game.tab_top[col] - 1) {
            g_Game.tableau[col][row] |= CARD_FACEUP;
            g_Game.score += 5;
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }

        if (g_Game.tableau[col][row] & CARD_FACEUP) {
            drag_count = g_Game.tab_top[col] - row;
            for (i = 0; i < drag_count; i++)
                drag_cards[i] = g_Game.tableau[col][row + i];

            drag_from_type = SRC_TAB;
            drag_from_idx = col;
            drag_x_off = mx - Layout_GetTabX(col);
            drag_y_off = my - Layout_GetTabCardY(col, row);
            dragging = TRUE;
            drag_mouse_x = mx;
            drag_mouse_y = my;
            SetCapture(hwnd);
            return;
        }
    }
}

void OnMouseMove(HWND hwnd, int mx, int my) {
    if (!dragging) return;
    drag_mouse_x = mx;
    drag_mouse_y = my;
    InvalidateRect(hwnd, NULL, FALSE);
}

void OnLButtonUp(HWND hwnd, int mx, int my) {
    int i, col;
    BOOL dropped = FALSE;
    CARD top_card;

    if (!dragging) return;

    top_card = drag_cards[0];
    dragging = FALSE;
    ReleaseCapture();

    /* Try Foundation */
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

    /* Try Tableau */
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

    /* Auto-flip */
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