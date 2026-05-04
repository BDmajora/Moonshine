#include "solitaire.h"
#include "sol_draw.h"
#include "sol_input.h" // Needed for DragState
#include "sol_timer.h"

/* Global Card Dimensions */
int g_CardWidth = 71;
int g_CardHeight = 96;

/**
 * DrawStockPile: Isolated rendering for the source deck.
 * This works because it draws bottom-to-top with small offsets.
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
 * DrawWastePile: Simplified to match Stock Pile logic.
 * No manual clipping. No complex loops. Just bottom-to-top rendering.
 */
static void DrawWastePile(HDC hdc)
{
    int i, show_count, hidden_count, base_offset;
    int wx_base = X_MARGIN + X_SPACING;
    const DragState* drag = Input_GetDragInfo(); // Fetch state safely

    if (g_Game.waste_top <= 0) return;

    show_count = (g_Game.waste_top < 3) ? g_Game.waste_top : 3;
    hidden_count = g_Game.waste_top - show_count;

    /* 1. Draw the staircase base first */
    if (hidden_count > 0) {
        // Draw bottom card
        cdtDraw(hdc, wx_base, Y_MARGIN, g_Game.waste[0] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
        
        // Draw a middle "step" if the stack is deep enough
        if (hidden_count > 5)
            cdtDraw(hdc, wx_base + 2, Y_MARGIN + 2, g_Game.waste[hidden_count/2] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
        
        base_offset = (hidden_count > 10) ? 4 : 2;
    } else {
        base_offset = 0;
    }

    /* 2. Draw the fanned cards on top of the base */
    for (i = 0; i < show_count; i++) {
        int fan_idx = (g_Game.waste_top - show_count) + i;
        int wx = (wx_base + base_offset) + (i * WASTE_FAN_OFF);
        int wy = Y_MARGIN + base_offset;

        if (drag->is_dragging && drag->from_type == SRC_WASTE && fan_idx == g_Game.waste_top - 1)
            continue;

        /* Let the DLL handle the rounded corners; we already cleared the background to green */
        cdtDraw(hdc, wx, wy, g_Game.waste[fan_idx] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
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
    RECT rcBoard, rcStatus, rcText;
    HBRUSH hbrGreen, hbrStatus;
    HPEN hpen, hOldPen;
    HFONT hfont, hOldFont;
    DWORD elapsed;
    const DragState* drag = Input_GetDragInfo(); // Fetch state safely

    /* 1. MANDATORY: Clear the entire board to green. 
       This fixes the black artifacts behind rounded corners. */
    SetRect(&rcBoard, 0, 0, width, height - STATUS_BAR_HEIGHT);
    hbrGreen = CreateSolidBrush(SOL_BG_COLOR);
    FillRect(hdc, &rcBoard, hbrGreen);
    DeleteObject(hbrGreen);

    /* 2. Draw the Status Bar */
    rcStatus.left = 0; 
    rcStatus.top = height - STATUS_BAR_HEIGHT;
    rcStatus.right = width; 
    rcStatus.bottom = height;
    
    hbrStatus = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rcStatus, hbrStatus);
    DeleteObject(hbrStatus);

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

    /* 3. Draw the Piles */
    DrawStockPile(hdc);
    DrawWastePile(hdc);

    /* Foundations */
    for (i = 0; i < 4; i++) {
        int fx = X_MARGIN + (3 + i) * X_SPACING;
        if (g_Game.found_top[i] == 0)
            cdtDraw(hdc, fx, Y_MARGIN, 0, MODE_GHOST, SOL_BG_COLOR);
        else
            cdtDraw(hdc, fx, Y_MARGIN, 
                    g_Game.foundation[i][g_Game.found_top[i] - 1] & ~CARD_FACEUP, 
                    MODE_FACEUP, SOL_BG_COLOR);
    }

    /* Tableau Columns */
    for (col = 0; col < 7; col++) {
        int cx = Layout_GetTabX(col);
        if (g_Game.tab_top[col] == 0) {
            cdtDraw(hdc, cx, Y_TABLEAU, 0, MODE_GHOST, SOL_BG_COLOR);
            continue;
        }

        y = Y_TABLEAU;
        for (row = 0; row < g_Game.tab_top[col]; row++) {
            CARD c = g_Game.tableau[col][row];
            
            /* If dragging, don't draw the cards that are in the drag stack */
            if (drag->is_dragging && drag->from_type == SRC_TAB && drag->from_idx == col
                    && row >= g_Game.tab_top[col] - drag->count)
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

    /* 4. Draw Dragging Stack last */
    if (drag->is_dragging) {
        int dx = drag->mouse_x - drag->x_off;
        int dy = drag->mouse_y - drag->y_off;
        for (i = 0; i < drag->count; i++)
            cdtDraw(hdc, dx, dy + (i * FACE_UP_OFF), 
                    drag->cards[i] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
    }

    EndGame_Draw(hdc, width, height);
}