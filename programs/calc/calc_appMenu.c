#include <windows.h>
#include <commctrl.h>
#include <wchar.h>
#include "calc.h"
#include "calc_ui.h"

/* ── Menu creation ──────────────────────────────────────────────── */
HMENU create_menu(void) {
    HMENU hBar  = CreateMenu();
    HMENU hView = CreatePopupMenu();
    HMENU hEdit = CreatePopupMenu();
    HMENU hHelp = CreatePopupMenu();
    HMENU hWork = CreatePopupMenu();

    AppendMenuW(hView, MF_STRING, ID_VIEW_STANDARD,   L"Standard\tAlt+1");
    AppendMenuW(hView, MF_STRING, ID_VIEW_SCIENTIFIC,  L"Scientific\tAlt+2");
    AppendMenuW(hView, MF_STRING, ID_VIEW_PROGRAMMER,  L"Programmer\tAlt+3");
    AppendMenuW(hView, MF_STRING, ID_VIEW_STATISTICS,  L"Statistics\tAlt+4");
    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING, ID_VIEW_HISTORY,     L"History\tCtrl+H");
    AppendMenuW(hView, MF_STRING, ID_VIEW_DIGIT_GRP,   L"Digit grouping");
    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING, ID_VIEW_BASIC,       L"Basic\tCtrl+F4");
    AppendMenuW(hView, MF_STRING, ID_PANEL_UNIT,       L"Unit conversion\tCtrl+U");
    AppendMenuW(hView, MF_STRING, ID_PANEL_DATE,       L"Date calculation\tCtrl+E");

    AppendMenuW(hWork, MF_STRING, ID_WS_MORTGAGE,  L"Mortgage");
    AppendMenuW(hWork, MF_STRING, ID_WS_VEHICLE,   L"Vehicle lease");
    AppendMenuW(hWork, MF_STRING, ID_WS_FUEL_MPG,  L"Fuel economy (mpg)");
    AppendMenuW(hWork, MF_STRING, ID_WS_FUEL_LKM,  L"Fuel economy (L/100 km)");
    AppendMenuW(hView, MF_POPUP,  (UINT_PTR)hWork, L"Worksheets");

    AppendMenuW(hEdit, MF_STRING, 500, L"Copy\tCtrl+C");
    AppendMenuW(hEdit, MF_STRING, 501, L"Paste\tCtrl+V");

    AppendMenuW(hHelp, MF_STRING, ID_HELP_ABOUT, L"About Calculator");

    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hView, L"View");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hEdit, L"Edit");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hHelp, L"Help");

    return hBar;
}

/* ── Window size helpers ────────────────────────────────────────── */
void get_window_size(int client_w, int client_h, DWORD style,
                            int *ww, int *wh) {
    RECT rc;
    rc.left   = 0;
    rc.top    = 0;
    rc.right  = client_w;
    rc.bottom = client_h;
    AdjustWindowRect(&rc, style, TRUE);
    *ww = rc.right  - rc.left;
    *wh = rc.bottom - rc.top;
}

void get_required_client_size(int mode, int panel, int *w, int *h) {
    int cols, rows, header_h;
    int margin, gap, disp_h, btn_w, btn_h;
    int grid_w, grid_h;
    
    margin = 12;
    gap    = 6;
    disp_h = 60;
    btn_w  = 40; 
    btn_h  = 32; 

    switch (mode) {
        case MODE_SCIENTIFIC: cols = 10; rows = 7; header_h = 30; break;
        case MODE_PROGRAMMER: cols = 9;  rows = 7; header_h = 50; break;
        case MODE_STATISTICS: cols = 5;  rows = 7; header_h = 100; break;
        default:              cols = 5;  rows = 6; header_h = 0; break;
    }

    grid_w = (cols * btn_w) + ((cols - 1) * gap);
    grid_h = (rows * btn_h) + ((rows - 1) * gap);

    *w = grid_w + (margin * 2);
    *h = margin + disp_h + margin + header_h + grid_h + margin;

    if (panel != PANEL_NONE) {
        *w += 260; 
    }
}

void apply_window_size(HWND hwnd, int mode, int panel) {
    int cw, ch;
    DWORD style;
    RECT rc;

    get_required_client_size(mode, panel, &cw, &ch);

    style = GetWindowLongW(hwnd, GWL_STYLE);
    rc.left = 0;
    rc.top = 0;
    rc.right = cw;
    rc.bottom = ch;
    
    AdjustWindowRect(&rc, style, TRUE);

    SetWindowPos(hwnd, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}