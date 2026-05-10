/* ────────────────────────────────────────────────────────────────────
   calc_appMenu.c — menu bar construction only.

   Window sizing helpers (get_window_size, get_required_client_size,
   apply_window_size) now live in calc_window.c.
   ──────────────────────────────────────────────────────────────────── */
#include <windows.h>
#include "calc.h"

HMENU create_menu(void) {
    HMENU hBar  = CreateMenu();
    HMENU hView = CreatePopupMenu();
    HMENU hEdit = CreatePopupMenu();
    HMENU hHelp = CreatePopupMenu();
    HMENU hWork = CreatePopupMenu();

    AppendMenuW(hView, MF_STRING, ID_VIEW_STANDARD,    L"Standard\tAlt+1");
    AppendMenuW(hView, MF_STRING, ID_VIEW_SCIENTIFIC,  L"Scientific\tAlt+2");
    AppendMenuW(hView, MF_STRING, ID_VIEW_PROGRAMMER,  L"Programmer\tAlt+3");
    AppendMenuW(hView, MF_STRING, ID_VIEW_STATISTICS,  L"Statistics\tAlt+4");
    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING, ID_VIEW_HISTORY,     L"History\tCtrl+H");
    AppendMenuW(hView, MF_STRING, ID_VIEW_DIGIT_GRP,   L"Digit grouping");
    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING, ID_VIEW_BASIC,       L"Basic\tCtrl+B");
    AppendMenuW(hView, MF_STRING, ID_PANEL_UNIT,       L"Unit conversion\tCtrl+U");
    AppendMenuW(hView, MF_STRING, ID_PANEL_DATE,       L"Date calculation\tCtrl+E");

    AppendMenuW(hWork, MF_STRING, ID_WS_MORTGAGE, L"Mortgage");
    AppendMenuW(hWork, MF_STRING, ID_WS_VEHICLE,  L"Vehicle lease");
    AppendMenuW(hWork, MF_STRING, ID_WS_FUEL_MPG, L"Fuel economy (mpg)");
    AppendMenuW(hWork, MF_STRING, ID_WS_FUEL_LKM, L"Fuel economy (L/100 km)");
    AppendMenuW(hView, MF_POPUP,  (UINT_PTR)hWork, L"Worksheets");

    AppendMenuW(hEdit, MF_STRING, 500, L"Copy\tCtrl+C");
    AppendMenuW(hEdit, MF_STRING, 501, L"Paste\tCtrl+V");
    AppendMenuW(hHelp, MF_STRING, ID_HELP_ABOUT, L"About Calculator");

    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hView, L"View");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hEdit, L"Edit");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hHelp, L"Help");
    return hBar;
}