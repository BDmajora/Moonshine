#include "solitaire.h"
#include "sol_draw.h"
#include "sol_input.h"  // For DragState access
#include "sol_timer.h"  // For Timer_GetElapsed()
#include "sol_layout.h" // For X_MARGIN, Y_MARGIN, offsets, etc.

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
 * DrawWastePile: Logic for the "fanned" waste stack.
 * Skips the top card if it is currently being dragged.
 */
static void DrawWastePile(HDC hdc)
{
    int i, show_count, hidden_count, base_offset;
    int wx_base = X_MARGIN + X_SPACING;
    const DragState* drag = Input_GetDragInfo(); 

    if (g_Game.waste_top <= 0) return;

    show_count = (g_Game.waste_top < 3) ? g_Game.waste_top : 3;
    hidden_count = g_Game.waste_top - show_count;

    /* 1. Draw the staircase base first */
    if (hidden_count > 0) {
        cdtDraw(hdc, wx_base, Y_MARGIN, g_Game.waste[0] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
        
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

        // SRP: Don't draw the card if the Input module says it's being dragged
        if (drag->is_dragging && drag->from_type == SRC_WASTE && fan_idx == g_Game.waste_top - 1)
            continue;

        cdtDraw(hdc, wx, wy, g_Game.waste[fan_idx] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
    }
}

/**
 * DrawBoard: Main orchestration of the rendering process.
 */
void DrawBoard(HDC hdc, int width, int height) {
    int i, col, row, y;
    WCHAR buf[128];
    RECT rcBoard, rcStatus, rcText;
    HBRUSH hbrGreen, hbrStatus;
    HPEN hpen, hOldPen;
    HFONT hfont, hOldFont;
    DWORD elapsed;
    const DragState* drag = Input_GetDragInfo(); 

    /* 1. Background Setup */
    SetRect(&rcBoard, 0, 0, width, height - STATUS_BAR_HEIGHT);
    hbrGreen = CreateSolidBrush(SOL_BG_COLOR);
    FillRect(hdc, &rcBoard, hbrGreen);
    DeleteObject(hbrGreen);

    /* 2. Status Bar Rendering */
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

    /* 3. Game Piles */
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
            
            /* Check Input state: skip cards currently held by mouse */
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

    /* 4. Dragging Overlay */
    if (drag->is_dragging) {
        int dx = drag->mouse_x - drag->x_off;
        int dy = drag->mouse_y - drag->y_off;
        for (i = 0; i < drag->count; i++)
            cdtDraw(hdc, dx, dy + (i * FACE_UP_OFF), 
                    drag->cards[i] & ~CARD_FACEUP, MODE_FACEUP, SOL_BG_COLOR);
    }

    /* 5. Cleanup / Effects */
    EndGame_Draw(hdc, width, height);
}