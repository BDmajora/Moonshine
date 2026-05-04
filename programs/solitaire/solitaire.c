#include "solitaire.h"
#include "sol_input.h"
#include "sol_endgame.h"

/* Global Card Dimensions (assigned during WM_CREATE) */
static int cardW, cardH;
static UINT timer_id;

LRESULT CALLBACK SolWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;
    MINMAXINFO *mmi;

    switch(msg) {
        case WM_CREATE:
            /* Restoration: Specific error handling for cards.dll */
            if(!cdtInit(&cardW, &cardH)) {
                MessageBoxW(hwnd, L"Failed to load cards.dll", L"Error", MB_ICONERROR);
                return -1;
            }
            Game_Init();
            timer_id = SetTimer(hwnd, 1, 1000, NULL);
            return 0;

        case WM_TIMER:
            /* High-level dispatcher handles both Game and EndGame timers */
            Input_OnTimer(hwnd, wp);
            return 0;

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            /* Restoration: Dispatcher handles Cheat (Alt+Shift+2) and ESC during EndGame */
            if (Input_OnKeyboard(hwnd, msg, wp, lp)) return 0;
            return DefWindowProcW(hwnd, msg, wp, lp);

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MOUSEMOVE:
            /* Dispatcher handles mouse input and EndGame dismissal/movement */
            Input_OnMouse(hwnd, msg, wp, lp);
            return 0;

        case WM_ERASEBKGND:
            /* Restoration: Critical for flicker-free endgame animation */
            if (g_endgame_active) return 1; 
            return 0;

        case WM_PAINT:
            hdc = BeginPaint(hwnd, &ps);
            GetClientRect(hwnd, &rc);
            
            /* Since DrawBoard (in sol_draw.c) calls EndGame_Draw internally, 
               we just call DrawBoard to keep the logic unified. */
            DrawBoard(hdc, rc.right, rc.bottom);
            
            EndPaint(hwnd, &ps);
            return 0;

        case WM_GETMINMAXINFO:
            /* Restoration: Prevents the layout from collapsing if the user resizes */
            mmi = (MINMAXINFO*)lp;
            mmi->ptMinTrackSize.x = 560;
            mmi->ptMinTrackSize.y = 400;
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
    WNDCLASSW wc = {0};
    RECT rc;
    /* Restoration: Specific Window Style (No resizing, fixed menu) */
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    /* Restoration: Calculate exact window size needed for the card layout */
    rc.left = 0; rc.top = 0;
    rc.right = X_MARGIN + (7 * X_SPACING);
    rc.bottom = 384;
    AdjustWindowRect(&rc, style, TRUE);

    wc.lpfnWndProc   = SolWndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"SolitaireWnd";
    wc.lpszMenuName  = MAKEINTRESOURCEW(IDR_MAINMENU);
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_SOLITAIRE));
    RegisterClassW(&wc);

    hwnd = CreateWindowW(L"SolitaireWnd", L"Solitaire", style,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         rc.right - rc.left, rc.bottom - rc.top,
                         NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}