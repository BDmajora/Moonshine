#include "solitaire.h"
#include "sol_input.h"
#include "sol_endgame.h"
#include "sol_res.h"
#include "sol_draw.h"
#include "sol_timer.h"

static int cardW, cardH;
static UINT timer_id;

/* Local callback for status bar refreshes */
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

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE:
        Input_OnMouse(hwnd, msg, wp, lp);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (Input_OnKeyboard(hwnd, msg, wp, lp)) return 0;
        break;

    case WM_COMMAND:
        Input_OnCommand(hwnd, LOWORD(wp));
        return 0;

    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &rc);
        // Encapsulates both the static board and the endgame overlay
        DrawBoard(hdc, rc.right, rc.bottom);
        EndPaint(hwnd, &ps);
        return 0;

    case WM_ERASEBKGND:
        // Returns 1 (handled) during endgame to allow the "card trail" effect
        if (g_endgame_active) return 1; 
        return 0;

    case WM_GETMINMAXINFO:
        Res_GetMinMaxInfo(lp);
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

    // Window creation and class registration handled by the Resource module
    hwnd = Res_InitWindow(hInstance, nCmdShow, SolWndProc);

    if (!hwnd) return 0;

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}