#include <windows.h>
#include "solitaire.h"

int cardW, cardHeight;

/* Updated height to 400 to snap perfectly to the bottom status bar */
#define X_MARGIN        12
#define X_SPACING       82
#define TARGET_CLIENT_HEIGHT 385

LRESULT CALLBACK SolWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;
    MINMAXINFO *mmi;
    HBRUSH hbr; 

    switch (msg) {
        case WM_CREATE:
            if (!cdtInit(&cardW, &cardHeight)) {
                MessageBoxW(hwnd, L"Failed to load cards.dll", L"Error", MB_ICONERROR);
                return -1;
            }
            InitGame();
            break;

        case WM_ERASEBKGND:
            return 1; 

        case WM_PAINT:
            hdc = BeginPaint(hwnd, &ps);
            GetClientRect(hwnd, &rc);
            
            /* Move hbr here to avoid C90 warning */
            hbr = CreateSolidBrush(SOL_BG_COLOR);
            FillRect(hdc, &rc, hbr);
            DeleteObject(hbr); 
            
            DrawBoard(hdc, rc.right, rc.bottom);
            EndPaint(hwnd, &ps);
            break;

        case WM_GETMINMAXINFO:
            mmi = (MINMAXINFO *)lp;
            mmi->ptMinTrackSize.x = 400; 
            mmi->ptMinTrackSize.y = 300;
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
    RECT rc;
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    int calculatedWidth = (X_MARGIN * 2) + (6 * X_SPACING) + 71; 

    rc.left = 0;
    rc.top = 0;
    rc.right = calculatedWidth;
    rc.bottom = TARGET_CLIENT_HEIGHT;

    AdjustWindowRect(&rc, dwStyle, TRUE);

    wc.lpfnWndProc = SolWndProc;
    wc.hInstance = hInst;
    wc.hbrBackground = NULL; 
    wc.lpszClassName = L"SolitaireWnd";
    wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MAINMENU);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_SOLITAIRE));

    RegisterClassW(&wc);

    hwnd = CreateWindowW(L"SolitaireWnd", L"Solitaire",
                         dwStyle, 
                         CW_USEDEFAULT, CW_USEDEFAULT, 
                         rc.right - rc.left, rc.bottom - rc.top, 
                         NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}