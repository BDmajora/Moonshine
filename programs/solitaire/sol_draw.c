#include "solitaire.h"
#include "sol_draw.h"
#include "sol_timer.h"

/* * Global Drag State Definition */
BOOL   dragging = FALSE;
CARD   drag_cards[52];
int    drag_count = 0;
int    drag_from_type;
int    drag_from_idx;
int    drag_mouse_x, drag_mouse_y;
int    drag_x_off, drag_y_off;

/**
 * DrawStockPile: Isolated rendering for the source deck.
 */
static void DrawStockPile(HDC hdc)
{
    if (g_Game.stock_top > 0) {
        cdtDraw(hdc, X_MARGIN, Y_MARGIN, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
        if (g_Game.stock_top > 5)
            cdtDraw(hdc, X_MARGIN + 2, Y_MARGIN + 2, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
        if (g_Game.stock_top > 10)
            cdtDraw(hdc, X_MARGIN + 4, Y_MARGIN + 4, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
    } else {
        if (g_Game.waste_top > 0)
            cdtDraw(hdc, X_MARGIN, Y_MARGIN, 0, MODE_DECKO, SOL_BG_COLOR); 
        else
            cdtDraw(hdc, X_MARGIN, Y_MARGIN, 0, MODE_DECKX, SOL_BG_COLOR); 
    }
}

/**
 * DrawWastePile: Isolated rendering for the discard pile.
 * FIX: Restores standard Left-to-Right Z-order (Top card is drawn last).
 * Uses GDI Clipping to eliminate the thick black artifact lines.
 */
static void DrawWastePile(HDC hdc)
{
    int i;
    int hidden_count;
    int show_count;
    int wx_base = X_MARGIN + X_SPACING;
    int base_offset;

    if (g_Game.waste_top <= 0) return;

    show_count = (g_Game.waste_top < 3) ? g_Game.waste_top : 3;
    hidden_count = g_Game.waste_top - show_count;
    base_offset = (hidden_count > 0) ? ((hidden_count - 1) / 10) : 0;
    if (base_offset > 4) base_offset = 4;

    /* 1. Draw hidden base cards (Bottom of the stack) */
    if (hidden_count > 0) {
        for (i = 0; i < hidden_count; i++) {
            int offset = i / 10;
            if (offset > 4) offset = 4;
            if (i == 0 || (i / 10 != (i - 1) / 10)) {
                cdtDraw(hdc, wx_base + offset, Y_MARGIN + offset, 
                        g_Game.waste[i] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
            }
        }
    }

    /* 2. Draw fanned visible cards (Correct Z-order: Left-to-Right) */
    for (i = 0; i < show_count; i++) {
        int fan_idx = g_Game.waste_top - show_count + i;
        int wx = (wx_base + base_offset) + (i * WASTE_FAN_OFF);
        int wy = (Y_MARGIN + base_offset);
        
        if (dragging && drag_from_type == SRC_WASTE && fan_idx == g_Game.waste_top - 1)
            continue;

        /* CRITICAL FIX: Save DC and IntersectClipRect to remove black bar artifacts */
        SaveDC(hdc);
        // We clip to the exact card dimensions (Standard cards.dll size is 71x96).
        // This prevents the library from drawing the "shadow" border that causes black lines.
        IntersectClipRect(hdc, wx, wy, wx + 71, wy + 96);
        
        cdtDraw(hdc, wx, wy, g_Game.waste[fan_idx] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
        
        RestoreDC(hdc, -1);
    }
}

void OnTimer(HWND hwnd) {
    if (Timer_IsRunning()) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        rc.top = rc.bottom - STATUS_BAR_HEIGHT;
        InvalidateRect(hwnd, &rc, FALSE);
    }
}

void DrawBoard(HDC hdc, int width, int height) {
    int i, col, row, y;
    WCHAR buf[128];
    RECT rcStatus, rcText;
    HBRUSH hbr;
    HPEN hpen, hOldPen;
    HFONT hfont, hOldFont;
    DWORD elapsed;

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

    elapsed = Timer_GetElapsed();
    wsprintfW(buf, L"Score: %d  Time: %u", g_Game.score, elapsed);
    
    rcText = rcStatus; 
    rcText.right -= STATUS_MARGIN;
    DrawTextW(hdc, buf, -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

    SelectObject(hdc, hOldFont);
    DeleteObject(hfont);

    DrawStockPile(hdc);
    DrawWastePile(hdc);

    for (i = 0; i < 4; i++) {
        int fx = X_MARGIN + (3 + i) * X_SPACING;
        if (g_Game.found_top[i] == 0)
            cdtDraw(hdc, fx, Y_MARGIN, 0, MODE_GHOST, SOL_BG_COLOR);
        else
            cdtDraw(hdc, fx, Y_MARGIN, 
                    g_Game.foundation[i][g_Game.found_top[i] - 1] & ~CARD_FACEUP, 
                    MODE_FACEUP, SOL_BG_COLOR);
    }

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

    if (dragging) {
        int dx = drag_mouse_x - drag_x_off;
        int dy = drag_mouse_y - drag_y_off;
        for (i = 0; i < drag_count; i++)
            cdtDraw(hdc, dx, dy + (i * FACE_UP_OFF), 
                    drag_cards[i] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
    }

    EndGame_Draw(hdc, width, height);
}