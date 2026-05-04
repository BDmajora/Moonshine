#include "solitaire.h"
#include "sol_input.h"
#include "sol_endgame.h"
#include "sol_res.h"  /* New Header */
#include "sol_draw.h" /* Needed for DrawBoard */
#include "sol_timer.h"/* Needed for Timer_IsRunning */

static int cardW, cardH;
static UINT timer_id;

/* Event handler for updating the status bar UI. 
   Called by the input module's timer dispatcher. */
void OnTimer(HWND hwnd) {
    if (Timer_IsRunning()) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        rc.top = rc.bottom - STATUS_BAR_HEIGHT;
        InvalidateRect(hwnd, &rc, FALSE);
    }
}

LRESULT CALLBACK SolWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;

    switch(msg) {
        case WM_CREATE:
            if(!cdtInit(&cardW, &cardH)) {
                MessageBoxW(hwnd, L"Failed to load cards.dll", L"Error", MB_ICONERROR);
                return -1;
            }
            Game_Init();
            timer_id = SetTimer(hwnd, 1, 1000, NULL);
            return 0;

        case WM_TIMER:
            Input_OnTimer(hwnd, wp);
            return 0;

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            if (Input_OnKeyboard(hwnd, msg, wp, lp)) return 0;
            return DefWindowProcW(hwnd, msg, wp, lp);

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MOUSEMOVE:
            Input_OnMouse(hwnd, msg, wp, lp);
            return 0;

        case WM_ERASEBKGND:
            if (g_endgame_active) return 1; 
            return 0;

        case WM_PAINT:
            hdc = BeginPaint(hwnd, &ps);
            GetClientRect(hwnd, &rc);
            DrawBoard(hdc, rc.right, rc.bottom);
            EndPaint(hwnd, &ps);
            return 0;

        case WM_GETMINMAXINFO:
            /* Restoration handled by Res module */
            Res_GetMinMaxInfo(lp);
            return 0;

        case WM_COMMAND:
            Input_OnCommand(hwnd, LOWORD(wp));
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, timer_id);
            cdtTerm();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    MSG msg;
    HWND hwnd;

    /* All setup and layout logic is now encapsulated here */
    hwnd = Res_InitWindow(hInstance, nCmdShow, SolWndProc);

    if (!hwnd) return 0;

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}