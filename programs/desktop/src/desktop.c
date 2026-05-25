/*
 * desktop.c — YetiOS desktop root window (wallpaper layer).
 *
 * Creates a single full-screen, borderless WS_POPUP window that paints
 * the wallpaper.  The frostedglass compositor detects this window by its
 * class/title and lowers it to the bottom of the scene graph, beneath the
 * taskbar and all application windows.
 */

#include <windows.h>

#include "desktop_defs.h"

/* ------------------------------------------------------------------ */
/* Module state                                                       */
/* ------------------------------------------------------------------ */

static HBRUSH g_bg_brush = NULL;

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/*
 * Returns the desired cursor for the desktop. 
 * Using this helper ensures that whenever we request a cursor, 
 * it is explicitly handled and ready for Wine's Wayland driver.
 */
static HCURSOR get_cursor(void) {
    return LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
}

/* ------------------------------------------------------------------ */
/* Window procedure                                                   */
/* ------------------------------------------------------------------ */

LRESULT CALLBACK desktop_wndproc(HWND hwnd, UINT msg,
    WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, g_bg_brush);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SETCURSOR:
        /*
         * Explicitly handle WM_SETCURSOR using our helper. 
         * By calling SetCursor and returning TRUE, we signal to Wine that 
         * the cursor is managed, which forces Wine to push the arrow 
         * cursor to the Wayland compositor.
         */
        if (LOWORD(lparam) == HTCLIENT) {
            SetCursor(get_cursor());
            return TRUE;
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);

    case WM_ERASEBKGND:
        /*
         * We paint the whole client area in WM_PAINT, so claim the
         * background as already erased.  Avoids a flash of the default
         * window-class background brush before our fill lands.
         */
        return 1;

    case WM_DISPLAYCHANGE: {
        /*
         * Resolution changed — resize to the new screen dimensions so we
         * keep covering the full desktop.  The compositor will re-pin us
         * to the bottom on the resulting commit.
         */
        int dw = GetSystemMetrics(SM_CXSCREEN);
        int dh = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, dw, dh,
            SWP_NOACTIVATE | SWP_NOZORDER);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEXW wc = {0};
    int sw, sh;
    HWND hwnd;
    MSG msg;

    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    g_bg_brush = CreateSolidBrush(
        RGB(DESKTOP_BG_R, DESKTOP_BG_G, DESKTOP_BG_B));

    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = desktop_wndproc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_DESKTOP));
    
    /* * Using get_cursor() here for the class default as well ensures
     * consistency from the moment the window is created.
     */
    wc.hCursor       = get_cursor();
    wc.hbrBackground = NULL;   /* we paint everything ourselves */
    wc.lpszClassName = L"" DESKTOP_WND_CLASS;

    if (!RegisterClassExW(&wc)) {
        return 1;
    }

    sw = GetSystemMetrics(SM_CXSCREEN);
    sh = GetSystemMetrics(SM_CYSCREEN);

    hwnd = CreateWindowExW(
        0,                              /* no extended styles */
        L"" DESKTOP_WND_CLASS,
        L"" DESKTOP_WND_TITLE,
        WS_POPUP | WS_VISIBLE,          /* borderless, no caption */
        0, 0, sw, sh,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        DeleteObject(g_bg_brush);
        return 1;
    }

    /* Push to the bottom of the Win32 z-order as a hint; the compositor
     * enforces the real scene-graph order. */
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_bg_brush);
    return (int)msg.wParam;
}