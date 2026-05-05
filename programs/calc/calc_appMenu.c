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

int mode_client_width(void) {
    int base;
    int panel_extra = (g_panel != PANEL_NONE) ? 280 : 0;
    switch (g_mode) {
        case MODE_SCIENTIFIC: base = 560; break;
        case MODE_PROGRAMMER: base = 580; break;
        default:              base = 300; break;
    }
    return base + panel_extra;
}

int mode_client_height(void) {
    switch (g_mode) {
        case MODE_SCIENTIFIC: return 440;
        case MODE_PROGRAMMER: return 460;
        case MODE_STATISTICS: return 460;
        default:              return 420;
    }
}