/* ────────────────────────────────────────────────────────────────────
   calc_window.c — window sizing.
   ──────────────────────────────────────────────────────────────────── */
#include <windows.h>
#include "calc.h"

#define GM_MARGIN   12
#define GM_GAP       6
#define GM_DISP_H   60
#define GM_BTN_W    44
#define GM_BTN_H    34
#define GM_RADIO_W  (GM_BTN_W + GM_BTN_W / 2)
#define GM_PANEL_W  260  /* Increased from 260 */

void get_window_size(int client_w, int client_h, DWORD style,
                     int *ww, int *wh) {
    RECT rc;
    rc.left = 0; rc.top = 0; rc.right = client_w; rc.bottom = client_h;
    AdjustWindowRect(&rc, style, TRUE);
    *ww = rc.right  - rc.left;
    *wh = rc.bottom - rc.top;
}

void get_required_client_size(int mode, int panel, int *w, int *h) {
    int btn_cols, btn_rows, header_h;
    int grid_w, grid_h;

    switch (mode) {
        case MODE_SCIENTIFIC:
            btn_cols = 10; btn_rows = 7; header_h = 30;
            grid_w = btn_cols * GM_BTN_W + (btn_cols - 1) * GM_GAP;
            break;
        case MODE_PROGRAMMER:
            btn_cols = 8;  btn_rows = 7; header_h = 50;
            grid_w = GM_RADIO_W + GM_GAP
                   + btn_cols * GM_BTN_W + (btn_cols - 1) * GM_GAP;
            break;
        case MODE_STATISTICS:
            btn_cols = 5;  btn_rows = 7; header_h = 100;
            grid_w = btn_cols * GM_BTN_W + (btn_cols - 1) * GM_GAP;
            break;
        default:
            btn_cols = 5;  btn_rows = 6; header_h = 0;
            grid_w = btn_cols * GM_BTN_W + (btn_cols - 1) * GM_GAP;
            break;
    }

    grid_h = btn_rows * GM_BTN_H + (btn_rows - 1) * GM_GAP;

    *w = GM_MARGIN * 2 + grid_w;
    *h = GM_MARGIN + GM_DISP_H + GM_MARGIN + header_h + grid_h + GM_MARGIN;

    if (panel != PANEL_NONE) *w += GM_PANEL_W;
}

void apply_window_size(HWND hwnd, int mode, int panel) {
    int cw, ch, ww, wh;
    DWORD style = (DWORD)GetWindowLongW(hwnd, GWL_STYLE);

    get_required_client_size(mode, panel, &cw, &ch);
    get_window_size(cw, ch, style, &ww, &wh);

    SetWindowPos(hwnd, NULL, 0, 0, ww, wh,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    /* Layout against the now-correct client rect. */
    ui_update_layout(hwnd);

    /* Force a full erase + repaint of the entire window and every
       child.  This nukes ghost pixels left behind when the window
       shrinks (e.g. Scientific → Standard): the old, larger area
       had button pixels that persist because WM_ERASEBKGND only
       fires for the invalidated region, which after a shrink is
       empty.  RedrawWindow with RDW_ERASE forces the background
       fill over the entire client rect. */
    RedrawWindow(hwnd, NULL, NULL,
                 RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}