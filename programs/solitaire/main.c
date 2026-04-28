#include <windows.h>
#include "solitaire.h"

/* Global Card Dimensions */
int cardW, cardHeight;

LRESULT CALLBACK SolWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    /* All variables declared at the top of the block for C90 compliance */
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;
    MINMAXINFO *mmi;
    HBRUSH hbr; 

    switch (msg) {
        case WM_CREATE:
            if (!cdtInit(&cardW, &cardHeight)) {
                MessageBoxW(hwnd, L"Failed to load cards.dll logic", L"Error", MB_ICONERROR);
                return -1;
            }
            InitGame();
            break;

        /* Fix: Prevents the "white flash" flicker when resizing or updating */
        case WM_ERASEBKGND:
            return 1; 

        case WM_PAINT:
            hdc = BeginPaint(hwnd, &ps);
            GetClientRect(hwnd, &rc);
            
            /* Optimized Background Draw */
            /* Fixed: Declaration of hbr moved to top of function */
            hbr = CreateSolidBrush(SOL_BG_COLOR);
            FillRect(hdc, &rc, hbr);
            DeleteObject(hbr); 
            
            DrawBoard(hdc, rc.right, rc.bottom);
            EndPaint(hwnd, &ps);
            break;

        case WM_GETMINMAXINFO:
            mmi = (MINMAXINFO *)lp;
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
    wc.hbrBackground = NULL; 
    wc.lpszClassName = L"SolitaireWnd";
    wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MAINMENU);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    
    /* Use the icon from your .rc file */
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_SOLITAIRE));

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