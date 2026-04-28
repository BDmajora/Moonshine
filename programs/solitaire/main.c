#include <windows.h>
#include "solitaire.h"

/* Global Card Dimensions */
int cardW, cardHeight;

LRESULT CALLBACK SolWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;
    MINMAXINFO *mmi;

    switch (msg) {
        case WM_CREATE:
            /* Initialize the cards.dll logic from your cards.c */
            if (!cdtInit(&cardW, &cardHeight)) {
                MessageBoxW(hwnd, L"Failed to load cards.dll logic", L"Error", MB_ICONERROR);
                return -1;
            }
            InitGame();
            break;

        case WM_PAINT:
            hdc = BeginPaint(hwnd, &ps);
            GetClientRect(hwnd, &rc);
            /* XP Green Fill */
            FillRect(hdc, &rc, CreateSolidBrush(SOL_BG_COLOR));
            DrawBoard(hdc, rc.right, rc.bottom);
            EndPaint(hwnd, &ps);
            break;

        case WM_GETMINMAXINFO:
            mmi = (MINMAXINFO *)lp;
            /* Lock the floor so the cards don't overlap awkwardly */
            mmi->ptMinTrackSize.x = 640;
            mmi->ptMinTrackSize.y = 480;
            break;

        case WM_COMMAND:
            if (LOWORD(wp) == IDM_GAME_EXIT) PostQuitMessage(0);
            if (LOWORD(wp) == IDM_GAME_DEAL) {
                InitGame();
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;

        case WM_DESTROY:
            cdtTerm();
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR cmd, int show) {
    WNDCLASSW wc = {0};
    HWND hwnd;
    MSG msg;

    wc.lpfnWndProc = SolWndProc;
    wc.hInstance = hInst;
    wc.hbrBackground = NULL; /* We handle background in WM_PAINT */
    wc.lpszClassName = L"SolitaireWnd";
    wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MAINMENU);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);

    RegisterClassW(&wc);

    hwnd = CreateWindowW(L"SolitaireWnd", L"Solitaire",
                         WS_OVERLAPPEDWINDOW, 
                         CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, 
                         NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}