#include "solitaire.h"
#include "sol_input.h"
#include "sol_endgame.h"
#include "sol_res.h"
#include "sol_draw.h"
#include "sol_timer.h"

static int cardW, cardH;
static UINT timer_id;

// Redraws the status bar area when the timer is active
void OnTimer(HWND hwnd) {
    if (Timer_IsRunning()) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        rc.top = rc.bottom - STATUS_BAR_HEIGHT;
        InvalidateRect(hwnd, &rc, FALSE);
    }
}

// Main window procedure for event handling
LRESULT CALLBACK SolWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;

    switch(msg) {
    case WM_CREATE:
        // Initialize card engine and game state
        if(!cdtInit(&cardW, &cardH)) {
            MessageBoxW(hwnd, L"Failed to load cards.dll", L"Error", MB_ICONERROR);
            return -1;
        }
        Game_Init();
        timer_id = SetTimer(hwnd, 1, 1000, NULL);
        return 0;

    case WM_TIMER:
        // Handle periodic UI updates and input logic
        if (wp == 1) OnTimer(hwnd);
        Input_OnTimer(hwnd, wp);
        return 0;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE:
        // Route mouse events to input module
        Input_OnMouse(hwnd, msg, wp, lp);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        // Handle keyboard shortcuts
        if (Input_OnKeyboard(hwnd, msg, wp, lp)) return 0;
        break;

    case WM_COMMAND:
        // Dispatch menu and button commands
        Input_OnCommand(hwnd, LOWORD(wp));
        return 0;

    case WM_PAINT:
        // Trigger board and endgame rendering
        hdc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &rc);
        DrawBoard(hdc, rc.right, rc.bottom);
        EndPaint(hwnd, &ps);
        return 0;

    case WM_ERASEBKGND:
        // Skip background erase during victory animation to avoid flicker
        return g_endgame_active ? 1 : 0;

    case WM_GETMINMAXINFO:
        // Apply window size constraints
        Res_GetMinMaxInfo(lp);
        return 0;

    case WM_DESTROY:
        // Cleanup resources and exit
        KillTimer(hwnd, timer_id);
        cdtTerm();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

// Win32 entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    MSG msg;
    HWND hwnd = Res_InitWindow(hInstance, nCmdShow, SolWndProc);

    if (!hwnd) return 0;

    // Standard message loop
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}