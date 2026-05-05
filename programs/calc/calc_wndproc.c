#include <windows.h>
#include <commctrl.h>
#include <wchar.h>
#include "calc.h"
#include "calc_logic.h"
#include "calc_ui.h"
#include "calc_panel.h"
#include "calc_about.h"

/* ── Sizing helper ────────────────────────────────────────────────── */
static void force_window_resize(HWND hwnd) {
    int cw = mode_client_width();
    int ch = mode_client_height();
    int ww, wh;
    DWORD dwStyle = (DWORD)GetWindowLongW(hwnd, GWL_STYLE);
    get_window_size(cw, ch, dwStyle, &ww, &wh);
    
    /* FIX: SetWindowPos with SWP_FRAMECHANGED forces the window manager to 
       re-calculate the frame and redrawing immediately, stopping the "jiggle" bug. */
    SetWindowPos(hwnd, NULL, 0, 0, ww, wh, 
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

/* ── Keyboard handling ───────────────────────────────────────────── */
static void on_keydown(HWND hwnd, WPARAM vk, BOOL ctrl, BOOL alt) {
    if (alt) {
        if (vk == '1') { on_command(hwnd, ID_VIEW_STANDARD);   return; }
        if (vk == '2') { on_command(hwnd, ID_VIEW_SCIENTIFIC);  return; }
        if (vk == '3') { on_command(hwnd, ID_VIEW_PROGRAMMER);  return; }
        if (vk == '4') { on_command(hwnd, ID_VIEW_STATISTICS);  return; }
        return;
    }
    if (ctrl) {
        if (vk == 'C') { on_command(hwnd, 500); return; }
        if (vk == 'V') { on_command(hwnd, 501); return; }
        if (vk == 'H') { on_command(hwnd, ID_VIEW_HISTORY); return; }
        if (vk == 'U') { on_command(hwnd, ID_PANEL_UNIT);   return; }
        if (vk == 'E') { on_command(hwnd, ID_PANEL_DATE);   return; }
        return;
    }
    switch (vk) {
        case VK_ESCAPE: on_command(hwnd, ID_CLR);  break;
        case VK_DELETE: on_command(hwnd, ID_CE);   break;
        case VK_BACK:   on_command(hwnd, ID_BACK); break;
        case VK_RETURN: on_command(hwnd, ID_EQ);   break;
        case VK_F9:     on_command(hwnd, ID_SIGN); break;
        default: break;
    }
}

static void on_char(HWND hwnd, WCHAR ch) {
    if (ch >= L'0' && ch <= L'9') { on_command(hwnd, ID_0 + (ch - L'0')); return; }
    if (ch >= L'a' && ch <= L'f') ch = (WCHAR)(ch - L'a' + L'A');
    switch (ch) {
        case L'A': on_command(hwnd, ID_A);       break;
        case L'B': on_command(hwnd, ID_B);       break;
        case L'C': on_command(hwnd, ID_C_HEX);  break;
        case L'D': on_command(hwnd, ID_D);       break;
        case L'E': on_command(hwnd, ID_E_HEX);  break;
        case L'F': on_command(hwnd, ID_F);       break;
        case L'+': on_command(hwnd, ID_ADD);     break;
        case L'-': on_command(hwnd, ID_SUB);     break;
        case L'*': on_command(hwnd, ID_MUL);     break;
        case L'/': on_command(hwnd, ID_DIV);     break;
        case L'.': on_command(hwnd, ID_DOT);     break;
        case L'=': on_command(hwnd, ID_EQ);      break;
        case L'%': on_command(hwnd, ID_PERCENT); break;
        case L'@': on_command(hwnd, ID_SQRT);    break;
        case 27:   on_command(hwnd, ID_CLR);     break;
        case 8:    on_command(hwnd, ID_BACK);    break;
        case 13:   on_command(hwnd, ID_EQ);      break;
        default:   break;
    }
}

/* ── Window Procedure ────────────────────────────────────────────── */
LRESULT CALLBACK CalcWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            ui_create_controls(hwnd);
            break;

        case WM_SIZE:
            ui_update_layout(hwnd);
            /* FIX: Tell the window to repaint immediately after layout logic 
               runs to prevent white gaps during manual resizing. */
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case WM_COMMAND: {
            int id    = (int)LOWORD(wp);
            int notif = (int)HIWORD(wp);
            if (id == ID_HELP_ABOUT) {
                About_ShowDialog(hwnd);
                break;
            }
            if (notif == CBN_SELCHANGE || notif == 0 || notif == BN_CLICKED) {
                on_command(hwnd, id);
                
                /* FIX: Check if we just switched a View mode. If so, trigger the 
                   frame resize helper so the user doesn't have to "jiggle" the window. */
                if (id >= ID_VIEW_STANDARD && id <= ID_VIEW_STATISTICS) {
                    force_window_resize(hwnd);
                }
            }
            if (id == ID_UNIT_FROM_VAL && notif == EN_CHANGE)
                panel_unit_convert(hwnd);
            break;
        }

        /* FIX: Prevent Wine/Windows from clearing the background with a 
           white brush before the app draws. This kills the flickering. */
        case WM_ERASEBKGND:
            return 1; 

        case WM_KEYDOWN: {
            BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            BOOL alt  = (GetKeyState(VK_MENU)    & 0x8000) != 0;
            on_keydown(hwnd, wp, ctrl, alt);
            break;
        }

        case WM_CHAR:
            on_char(hwnd, (WCHAR)wp);
            break;

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO *)lp;
            DWORD style = (DWORD)GetWindowLongW(hwnd, GWL_STYLE);
            int mw = (g_mode == MODE_STANDARD ? 280 : 420);
            int mh = 380;
            int ww, wh;
            get_window_size(mw, mh, style, &ww, &wh);
            mmi->ptMinTrackSize.x = ww;
            mmi->ptMinTrackSize.y = wh;
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

/* ── Entry point ─────────────────────────────────────────────────── */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev,
                    LPWSTR cmdline, int cmdshow) {
    WNDCLASSW  wc;
    HWND       hwnd;
    MSG        msg;
    HMENU      hMenu;
    
    /* FIX: WS_CLIPCHILDREN is the most important change. It stops the parent 
       window from painting over the buttons, which eliminates the white flickering 
       and unresponsiveness during movement in Wine. */
    DWORD      dwStyle = (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN) & ~WS_MAXIMIZEBOX;
    
    int        cw, ch, ww, wh;
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
    cw = mode_client_width();
    ch = mode_client_height();
    get_window_size(cw, ch, dwStyle, &ww, &wh);

    hwnd = CreateWindowW(L"CalcWnd", L"Calculator",
                         dwStyle,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         ww, wh,
                         NULL, hMenu, hInstance, NULL);

    ui_update_menu_check(hMenu);
    ShowWindow(hwnd, cmdshow);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}