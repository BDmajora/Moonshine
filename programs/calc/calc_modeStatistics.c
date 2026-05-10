/* ────────────────────────────────────────────────────────────────────
   calc_modeStatistics.c — Statistics mode controls + layout.

   The previous implementation placed the listbox BELOW the display
   and arrow buttons floated awkwardly to its right.  In the Windows
   reference the listbox is ABOVE the display, with the arrow
   buttons tucked into the top-right corner of the listbox area.
   This module recreates that layout.

   The display also needs to move DOWN to make room for the listbox,
   so this module's layout overrides the display position itself.
   ──────────────────────────────────────────────────────────────────── */
#include "calc_modeStatistics.h"

extern HWND make_button(HWND p, int id, const WCHAR *lbl, BOOL def);
extern void ui_register_child(HWND h);

#define LY_LIST_H   90    /* listbox height */
#define LY_ARROW_W  26    /* width of ▲ / ▼ arrow buttons */

void mode_stat_create(HWND hwnd) {
    HWND h;

    /* Listbox for the data series */
    h = CreateWindowW(L"LISTBOX", NULL,
                      WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                      0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_DISP_HISTORY,
                      NULL, NULL);
    ui_register_child(h);

    /* Up / Down arrow buttons (custom IDs 400/401) */
    h = CreateWindowW(L"BUTTON", L"\x25b2",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)400, NULL, NULL);
    ui_register_child(h);
    h = CreateWindowW(L"BUTTON", L"\x25bc",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)401, NULL, NULL);
    ui_register_child(h);

    /* Memory row */
    make_button(hwnd, ID_MC,     L"MC",      FALSE);
    make_button(hwnd, ID_MR,     L"MR",      FALSE);
    make_button(hwnd, ID_MS,     L"MS",      FALSE);
    make_button(hwnd, ID_MPLUS,  L"M+",      FALSE);
    make_button(hwnd, ID_MMINUS, L"M\x2212", FALSE);

    /* Row 1: ← CAD C F-E Exp */
    make_button(hwnd, ID_BACK,     L"\x2190", FALSE);
    make_button(hwnd, ID_STAT_CAD, L"CAD",    FALSE);
    make_button(hwnd, ID_CLR,      L"C",      FALSE);
    make_button(hwnd, ID_SCI_FE,   L"F-E",    FALSE);
    make_button(hwnd, ID_SCI_EXP,  L"Exp",    FALSE);

    /* Row 2: 7 8 9 x̄ x̄² */
    make_button(hwnd, ID_7,          L"7",          FALSE);
    make_button(hwnd, ID_8,          L"8",          FALSE);
    make_button(hwnd, ID_9,          L"9",          FALSE);
    make_button(hwnd, ID_STAT_MEAN,  L"x\x305",     FALSE);
    make_button(hwnd, ID_STAT_MEAN2, L"x\x305\xb2", FALSE);

    /* Row 3: 4 5 6 Σx Σx² */
    make_button(hwnd, ID_4,          L"4",         FALSE);
    make_button(hwnd, ID_5,          L"5",         FALSE);
    make_button(hwnd, ID_6,          L"6",         FALSE);
    make_button(hwnd, ID_STAT_SUMX,  L"\x3a3x",    FALSE);
    make_button(hwnd, ID_STAT_SUMX2, L"\x3a3x\xb2",FALSE);

    /* Row 4: 1 2 3 σn σn-1 */
    make_button(hwnd, ID_1,          L"1",        FALSE);
    make_button(hwnd, ID_2,          L"2",        FALSE);
    make_button(hwnd, ID_3,          L"3",        FALSE);
    make_button(hwnd, ID_STAT_SDEV,  L"\x3c3n",   FALSE);
    make_button(hwnd, ID_STAT_SDEV1, L"\x3c3n-1", FALSE);

    /* Row 5: 0 . ± Add (spans 2) */
    make_button(hwnd, ID_0,        L"0",   FALSE);
    make_button(hwnd, ID_DOT,      L".",   FALSE);
    make_button(hwnd, ID_SIGN,     L"\xb1",FALSE);
    make_button(hwnd, ID_STAT_ADD, L"Add", TRUE);
}

void mode_stat_layout(HWND hwnd, Layout *L) {
    int x   = L->disp_x;
    int g   = L->gap;
    int bw  = L->bw;
    int bh  = L->bh;
    int list_w = L->grid_w - LY_ARROW_W - g;
    int top_y = L->disp_y;
    int disp_y, grid_y, y;
    int row;

    /* Listbox at top, full width minus the arrow column */
    move_ctrl(hwnd, ID_DISP_HISTORY, x, top_y, list_w, LY_LIST_H);

    /* Arrow buttons stacked in the top-right corner */
    move_ctrl(hwnd, 400, x + list_w + g, top_y,                      LY_ARROW_W, LY_LIST_H / 2 - g / 2);
    move_ctrl(hwnd, 401, x + list_w + g, top_y + LY_LIST_H / 2 + g/2, LY_ARROW_W, LY_LIST_H / 2 - g / 2);

    /* Display BELOW the listbox.  Override the display rect that
       compute_layout positioned higher up. */
    disp_y = top_y + LY_LIST_H + g;
    move_ctrl(hwnd, ID_DISPLAY, x, disp_y, L->grid_w, L->disp_h);

    /* Button grid below the display */
    grid_y = disp_y + L->disp_h + g;
    y = grid_y;
    row = 0;

    move_ctrl(hwnd, ID_MC,         x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MR,         x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MS,         x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MPLUS,      x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_MMINUS,     x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_BACK,       x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_CAD,   x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_CLR,        x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_SCI_FE,     x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_SCI_EXP,    x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_7,          x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_8,          x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_9,          x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_MEAN,  x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_MEAN2, x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_4,          x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_5,          x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_6,          x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_SUMX,  x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_SUMX2, x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_1,          x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_2,          x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_3,          x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_SDEV,  x+(bw+g)*3, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_SDEV1, x+(bw+g)*4, y+(bh+g)*row, bw, bh); row++;

    move_ctrl(hwnd, ID_0,          x+(bw+g)*0, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_DOT,        x+(bw+g)*1, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_SIGN,       x+(bw+g)*2, y+(bh+g)*row, bw, bh);
    move_ctrl(hwnd, ID_STAT_ADD,   x+(bw+g)*3, y+(bh+g)*row, bw*2+g, bh);
}

void mode_stat_get_grid(int *cols, int *rows, int *header_h) {
    /* 5 cols × 6 rows for the bottom button grid.  The header
       reservation covers BOTH the listbox AND the gap to the display.
       The display itself sits inside this header band — that's why
       compute_layout in calc_update.c doesn't need to know about it.
       mode_stat_layout overrides the display position to put it
       below the listbox. */
    *cols = 5; *rows = 6; *header_h = LY_LIST_H + 6;
}