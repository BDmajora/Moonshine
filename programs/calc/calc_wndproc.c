#include "calc_wndproc.h"
#include <windows.h>
#include <commctrl.h>
#include <wchar.h>
#include "calc.h"
#include "calc_logic.h"
#include "calc_ui.h"
#include "calc_panel.h"
#include "calc_about.h"
#include "calc_hotkeys.h"

extern int g_mode;
extern int g_panel;

/* Keyboard handling moved to calc_hotkeys.c — Hotkeys_OnKey returns
   TRUE if the key was handled, in which case WndProc skips
   DefWindowProc and IsDialogMessage to avoid the Alt key opening
   menus before our shortcuts run. */

LRESULT CALLBACK CalcWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            ui_create_controls(hwnd);
            break;

        case WM_SIZE:
            ui_update_layout(hwnd);
            break;

        case WM_COMMAND: {
            int id    = (int)LOWORD(wp);
            int notif = (int)HIWORD(wp);

            if (id == ID_HELP_ABOUT) {
                About_ShowDialog(hwnd);
                break;
            }
            if (id == ID_UNIT_FROM_VAL && notif == EN_CHANGE) {
                panel_unit_convert(hwnd);
                break;
            }
            if (notif == CBN_SELCHANGE || notif == BN_CLICKED || notif == 0)
                on_command(hwnd, id);
            break;
        }

        case WM_ERASEBKGND: {
            /* Actually erase the background — the old "return 1" was a
               lie that left ghost pixels from destroyed controls when
               the window shrank (e.g. Scientific → Standard). */
            HDC hdc = (HDC)wp;
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, (HBRUSH)(COLOR_BTNFACE + 1));
            return 1;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            /* WM_SYSKEYDOWN fires for Alt-combinations.  Hotkeys_OnKey
               handles both, returning TRUE if the key was a known
               shortcut.  We still fall through to DefWindowProc when
               unhandled so menu navigation (Alt to focus menu bar)
               keeps working. */
            if (Hotkeys_OnKey(hwnd, msg, wp, lp))
                return 0;
            return DefWindowProcW(hwnd, msg, wp, lp);

        case WM_CHAR:
            if (Hotkeys_OnKey(hwnd, msg, wp, lp))
                return 0;
            break;

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO *)lp;
            DWORD style = (DWORD)GetWindowLongW(hwnd, GWL_STYLE);
            int cw, ch, ww, wh;
            get_required_client_size(g_mode, g_panel, &cw, &ch);
            get_window_size(cw, ch, style, &ww, &wh);
            mmi->ptMinTrackSize.x = ww;
            mmi->ptMinTrackSize.y = wh;
            mmi->ptMaxTrackSize.x = ww;
            mmi->ptMaxTrackSize.y = wh;
            return 0;
        }

        case WM_DESTROY:
            if (hBtnFont)   DeleteObject(hBtnFont);
            if (hDispFont)  DeleteObject(hDispFont);
            if (hSmallFont) DeleteObject(hSmallFont);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev,
                    LPWSTR cmdline, int cmdshow) {
    WNDCLASSW wc;
    HWND hwnd;
    MSG  msg;
    HMENU hMenu;
    DWORD dwStyle = (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS)
                    & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
    int cw, ch, ww, wh;
    INITCOMMONCONTROLSEX icc;

    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = CalcWndProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"CalcWnd";
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CALC));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    RegisterClassW(&wc);

    hMenu = create_menu();
    get_required_client_size(g_mode, g_panel, &cw, &ch);
    get_window_size(cw, ch, dwStyle, &ww, &wh);

    hwnd = CreateWindowW(L"CalcWnd", L"Calculator", dwStyle,
                         CW_USEDEFAULT, CW_USEDEFAULT, ww, wh,
                         NULL, hMenu, hInstance, NULL);

    ui_update_menu_check(hMenu);
    ShowWindow(hwnd, cmdshow);
    apply_window_size(hwnd, g_mode, g_panel);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        /* Hotkeys win first: IsDialogMessageW eats Ctrl+F4 (close child),
           Tab navigation, and various F-keys.  Translating our shortcuts
           here ensures the wndproc receives them.  We only consult
           Hotkeys_OnKey for relevant messages addressed to our window. */
        if ((msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) &&
            msg.hwnd != NULL &&
            (msg.hwnd == hwnd || GetAncestor(msg.hwnd, GA_ROOT) == hwnd) &&
            Hotkeys_OnKey(hwnd, msg.message, msg.wParam, msg.lParam)) {
            continue;
        }
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    (void)hPrev; (void)cmdline;
    return (int)msg.wParam;
}