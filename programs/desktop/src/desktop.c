/*
 * desktop.c — YetiOS desktop root window (wallpaper layer).
 *
 * Creates a single full-screen, borderless WS_POPUP window that paints
 * the wallpaper. The frostedglass compositor detects this window by its
 * class/title and lowers it to the bottom of the scene graph.
 */

#include <windows.h>
#include "desktop_defs.h"

/* ------------------------------------------------------------------ */
/* Module state                                                       */
/* ------------------------------------------------------------------ */

static HBRUSH g_bg_brush = NULL;

/* ------------------------------------------------------------------ */
/* Window procedure                                                   */
/* ------------------------------------------------------------------ */

LRESULT CALLBACK desktop_wndproc(HWND hwnd, UINT msg,
    WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        FillRect(dc, &rc, g_bg_brush);

        EndPaint(hwnd, &ps);
        return 0;
    }

    /*
     * IMPORTANT:
     * Let Wine handle cursor routing completely.
     * Do NOT override with SetCursor or custom logic.
     */
    case WM_SETCURSOR:
        return DefWindowProcW(hwnd, msg, wparam, lparam);

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    {
        SetActiveWindow(hwnd);
        SetFocus(hwnd);
        return 0;
    }

    case WM_CONTEXTMENU:
        return 0;

    case WM_DISPLAYCHANGE:
    {
        int dw = GetSystemMetrics(SM_CXSCREEN);
        int dh = GetSystemMetrics(SM_CYSCREEN);

        SetWindowPos(hwnd, HWND_BOTTOM,
            0, 0, dw, dh,
            SWP_NOACTIVATE);

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
    LPWSTR lpCmdLine, int nCmdShow)
{
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

    /*
     * WINE-COMPATIBLE CURSOR FIX
     * This is the only form that matches your headers.
     */
    wc.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));

    wc.hbrBackground = NULL;
    wc.lpszClassName = L"" DESKTOP_WND_CLASS;
    wc.style = CS_DBLCLKS;

    if (!RegisterClassExW(&wc))
        return 1;

    sw = GetSystemMetrics(SM_CXSCREEN);
    sh = GetSystemMetrics(SM_CYSCREEN);

    hwnd = CreateWindowExW(
        0,
        L"" DESKTOP_WND_CLASS,
        L"" DESKTOP_WND_TITLE,
        WS_POPUP | WS_VISIBLE,
        0, 0, sw, sh,
        NULL, NULL, hInstance, NULL);

    if (!hwnd)
    {
        DeleteObject(g_bg_brush);
        return 1;
    }

    /*
     * Keep desktop visually bottom-most WITHOUT breaking input model.
     */
    SetWindowPos(hwnd, HWND_BOTTOM,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE);

    SetActiveWindow(hwnd);
    SetFocus(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_bg_brush);
    return (int)msg.wParam;
}