#include "solitaire.h"
#include "sol_draw.h"
#include "sol_timer.h" // Added timer dependency

/* * Global Drag State Definition 
 * (Allocates memory for variables declared 'extern' in solitaire.h)
 */
BOOL  dragging = FALSE;
CARD  drag_cards[52];
int   drag_count = 0;
int   drag_from_type;
int   drag_from_idx;
int   drag_mouse_x, drag_mouse_y;
int   drag_x_off, drag_y_off;

/**
 * OnTimer: Handles status bar updates. 
 * Note: Only invalidates if the timer is actually running to save resources.
 */
void OnTimer(HWND hwnd) {
    if (Timer_IsRunning()) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        rc.top = rc.bottom - STATUS_BAR_HEIGHT;
        InvalidateRect(hwnd, &rc, FALSE);
    }
}

/**
 * DrawBoard: Main rendering entry point for the solitaire table.
 */
void DrawBoard(HDC hdc, int width, int height) {
    int i, col, row, y;
    WCHAR buf[128];
    RECT rcStatus, rcText;
    HBRUSH hbr;
    HPEN hpen, hOldPen;
    HFONT hfont, hOldFont;
    DWORD elapsed;

    /* 1. Status Bar Rendering */
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

    /* --- FIX: Use the new Timer Module --- */
    elapsed = Timer_GetElapsed();
    wsprintfW(buf, L"Score: %d  Time: %u", g_Game.score, elapsed);
    /* ------------------------------------- */
    
    rcText = rcStatus; 
    rcText.right -= STATUS_MARGIN;
    DrawTextW(hdc, buf, -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

    SelectObject(hdc, hOldFont);
    DeleteObject(hfont);

    /* 2. Stock - 3D Thickness Effect (Top/Left Fanning) */
    if (g_Game.stock_top > 0) {
        // Base Layer (Anchor)
        cdtDraw(hdc, X_MARGIN, Y_MARGIN, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
        
        // Middle Thickness (5+ cards)
        if (g_Game.stock_top > 5)
            cdtDraw(hdc, X_MARGIN + 2, Y_MARGIN + 2, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
            
        // Top Card (10+ cards)
        if (g_Game.stock_top > 10)
            cdtDraw(hdc, X_MARGIN + 4, Y_MARGIN + 4, CARD_BACK_RED, MODE_FACEDOWN, SOL_BG_COLOR);
    } else {
        /* Render markers for empty deck */
        if (g_Game.waste_top > 0)
            cdtDraw(hdc, X_MARGIN, Y_MARGIN, 0, MODE_DECKO, SOL_BG_COLOR); 
        else
            cdtDraw(hdc, X_MARGIN, Y_MARGIN, 0, MODE_DECKX, SOL_BG_COLOR); 
    }

    /* 3. Waste Pile - Subtle 1-pixel Vertical Fan */
    if (g_Game.waste_top > 0) {
        int show = g_Game.waste_top < 3 ? g_Game.waste_top : 3;
        int wx_base = X_MARGIN + X_SPACING;
        
        for (i = 0; i < show; i++) {
            int wx = wx_base + (i * WASTE_FAN_OFF);
            int wy = Y_MARGIN + i; 
            
            // Skip rendering the card if it's currently being dragged
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

    /* 5. Tableau Columns */
    for (col = 0; col < 7; col++) {
        int cx = Layout_GetTabX(col);
        if (g_Game.tab_top[col] == 0) {
            cdtDraw(hdc, cx, Y_TABLEAU, 0, MODE_GHOST, SOL_BG_COLOR);
            continue;
        }

        y = Y_TABLEAU;
        for (row = 0; row < g_Game.tab_top[col]; row++) {
            CARD c = g_Game.tableau[col][row];

            // Don't draw cards that are currently in the drag stack
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

    /* 6. Drag Stack - Always rendered last to stay on top */
    if (dragging) {
        int dx = drag_mouse_x - drag_x_off;
        int dy = drag_mouse_y - drag_y_off;
        for (i = 0; i < drag_count; i++)
            cdtDraw(hdc, dx, dy + (i * FACE_UP_OFF), 
                    drag_cards[i] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
    }

    /* 7. End-Game Animation Overlay Rendering */
    EndGame_Draw(hdc, width, height);
}