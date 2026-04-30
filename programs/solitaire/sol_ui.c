#include "solitaire.h"

/* Spacing constants to match reference image_0e1d5b.png */
#define TAB_FACE_DOWN_OFF  3   /* Tight packing for hidden cards */
#define TAB_FACE_UP_OFF   15   /* Standard spacing for visible cards */
#define STATUS_MARGIN      12

/* Drag state */
static BOOL  dragging = FALSE;
static CARD  drag_cards[52];
static int   drag_count = 0;
static int   drag_from_type;   /* 1: Waste, 2: Tab, 3: Found */
static int   drag_from_idx;
static int   drag_mouse_x, drag_mouse_y;
static int   drag_x_off, drag_y_off;

#define SRC_WASTE  1
#define SRC_TAB    2
#define SRC_FOUND  3

#define CARD_BACK_RED 54 /* Red back as seen in image_0e1a13.png */

void OnTimer(HWND hwnd) {
    /* Update the clock area in the status bar */
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

    /* 1. Draw Status Bar (The white bar at the bottom) */
    rcStatus.left = 0; rcStatus.top = height - STATUS_BAR_HEIGHT;
    rcStatus.right = width; rcStatus.bottom = height;
    hbr = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rcStatus, hbr); 
    DeleteObject(hbr);

    /* Separator line for status bar */
    hpen = CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
    hOldPen = (HPEN)SelectObject(hdc, hpen);
    MoveToEx(hdc, 0, height - STATUS_BAR_HEIGHT, NULL);
    LineTo(hdc, width, height - STATUS_BAR_HEIGHT);
    SelectObject(hdc, hOldPen); DeleteObject(hpen);

    /* Create Bold Font for Score/Time - Bumped to size 16 */
    hfont = CreateFontW(16, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"MS Sans Serif");
    hOldFont = (HFONT)SelectObject(hdc, hfont);
    SetBkMode(hdc, TRANSPARENT);

    /* Combine Score and Time into one string for bottom-right placement */
    elapsed = (GetTickCount() - g_Game.start_tick) / 1000;
    wsprintfW(buf, L"Score: %d    Time: %d", g_Game.score, (int)elapsed);
    
    rcText = rcStatus;
    rcText.right -= STATUS_MARGIN;
    DrawTextW(hdc, buf, -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

    SelectObject(hdc, hOldFont); DeleteObject(hfont);

    /* 2. Draw Stock (3D depth effect) */
    if (g_Game.stock_top > 0) {
        if (g_Game.stock_top > 5) 
            cdtDraw(hdc, X_MARGIN - 4, Y_MARGIN - 4, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
        if (g_Game.stock_top > 1) 
            cdtDraw(hdc, X_MARGIN - 2, Y_MARGIN - 2, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
        cdtDraw(hdc, X_MARGIN, Y_MARGIN, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
    } else {
        cdtDraw(hdc, X_MARGIN, Y_MARGIN, 0, MODE_GHOST, SOL_BG_COLOR);
    }

    /* 3. Draw Waste */
    if (g_Game.waste_top > 0)
        cdtDraw(hdc, X_MARGIN + X_SPACING, Y_MARGIN, g_Game.waste[g_Game.waste_top - 1] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);

    /* 4. Draw Foundations */
    for (i = 0; i < 4; i++) {
        int fx = X_MARGIN + (3 + i) * X_SPACING;
        if (g_Game.found_top[i] == 0)
            cdtDraw(hdc, fx, Y_MARGIN, 0, MODE_GHOST, SOL_BG_COLOR);
        else
            cdtDraw(hdc, fx, Y_MARGIN, g_Game.foundation[i][g_Game.found_top[i] - 1] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
    }

    /* 5. Draw Tableau (Packed Layout) */
    for (col = 0; col < 7; col++) {
        int cx = Layout_GetTabX(col);
        if (g_Game.tab_top[col] == 0) {
            cdtDraw(hdc, cx, Y_TABLEAU, 0, MODE_GHOST, SOL_BG_COLOR);
            continue;
        }
        
        y = Y_TABLEAU;
        for (row = 0; row < g_Game.tab_top[col]; row++) {
            CARD c = g_Game.tableau[col][row];
            if (dragging && drag_from_type == SRC_TAB && drag_from_idx == col && row >= g_Game.tab_top[col] - drag_count)
                break;

            if (c & CARD_FACEUP) {
                cdtDraw(hdc, cx, y, c & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
                y += TAB_FACE_UP_OFF;
            } else {
                cdtDraw(hdc, cx, y, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
                y += TAB_FACE_DOWN_OFF; /* PACKED spacing */
            }
        }
    }

    /* 6. Draw Dragging Stack */
    if (dragging) {
        int dx = drag_mouse_x - drag_x_off;
        int dy = drag_mouse_y - drag_y_off;
        for (i = 0; i < drag_count; i++) {
            cdtDraw(hdc, dx, dy + (i * TAB_FACE_UP_OFF), drag_cards[i] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
        }
    }
}

void OnLButtonDown(HWND hwnd, int mx, int my) {
    int col, row, i;

    /* Stock Click */
    if (Layout_CheckHit(mx, my, X_MARGIN, Y_MARGIN)) {
        if (g_Game.stock_top > 0) {
            g_Game.waste[g_Game.waste_top++] = g_Game.stock[--g_Game.stock_top] | CARD_FACEUP;
        } else {
            while (g_Game.waste_top > 0) 
                g_Game.stock[g_Game.stock_top++] = g_Game.waste[--g_Game.waste_top] & ~CARD_FACEUP;
            g_Game.score = (g_Game.score > 100) ? g_Game.score - 100 : 0;
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    /* Waste Click */
    if (g_Game.waste_top > 0 && Layout_CheckHit(mx, my, X_MARGIN + X_SPACING, Y_MARGIN)) {
        drag_cards[0] = g_Game.waste[g_Game.waste_top - 1];
        drag_count = 1;
        drag_from_type = SRC_WASTE;
        drag_x_off = mx - (X_MARGIN + X_SPACING);
        drag_y_off = my - Y_MARGIN;
        dragging = TRUE;
        drag_mouse_x = mx; drag_mouse_y = my;
        SetCapture(hwnd);
        return;
    }

    /* Tableau Click */
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
            drag_mouse_x = mx; drag_mouse_y = my;
            SetCapture(hwnd);
            return;
        }
    }
}

void OnMouseMove(HWND hwnd, int mx, int my) {
    if (!dragging) return;
    drag_mouse_x = mx; drag_mouse_y = my;
    InvalidateRect(hwnd, NULL, FALSE);
}

void OnLButtonUp(HWND hwnd, int mx, int my) {
    int i, col;
    BOOL dropped = FALSE;
    CARD top_card; /* C90 compatible */

    if (!dragging) return;
    
    top_card = drag_cards[0]; 
    dragging = FALSE;
    ReleaseCapture();

    /* 1. Try Foundation Drop */
    for (i = 0; i < 4; i++) {
        int fx = X_MARGIN + (3 + i) * X_SPACING;
        if (drag_count == 1 && Layout_CheckHit(mx, my, fx, Y_MARGIN) && Game_CanDropFound(top_card, i)) {
            g_Game.foundation[i][g_Game.found_top[i]++] = top_card;
            if (drag_from_type == SRC_WASTE) g_Game.waste_top--;
            else if (drag_from_type == SRC_TAB) g_Game.tab_top[drag_from_idx]--;
            
            g_Game.score += 10;
            dropped = TRUE; break;
        }
    }

    /* 2. Try Tableau Drop */
    if (!dropped) {
        for (col = 0; col < 7; col++) {
            int cx = Layout_GetTabX(col);
            int n = g_Game.tab_top[col];
            int ty = (n == 0) ? Y_TABLEAU : Layout_GetTabCardY(col, n - 1);

            if (mx >= cx && mx < cx + 71 && my >= ty && my < ty + 96) {
                if (Game_CanDropTab(top_card, col)) {
                    for (i = 0; i < drag_count; i++)
                        g_Game.tableau[col][g_Game.tab_top[col]++] = drag_cards[i];

                    if (drag_from_type == SRC_WASTE) { g_Game.waste_top--; g_Game.score += 5; }
                    else if (drag_from_type == SRC_TAB) g_Game.tab_top[drag_from_idx] -= drag_count;

                    dropped = TRUE; break;
                }
            }
        }
    }

    /* Auto-flip logic if a column is cleared */
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