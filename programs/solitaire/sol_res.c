#include "solitaire.h"
#include "sol_res.h"

HWND Res_InitWindow(HINSTANCE hInst, int nCmdShow, WNDPROC lpfnWndProc)
{
    WNDCLASSW wc = {0};
    HWND hwnd;
    RECT rc;
    
    /* Strict Window Style: No resizing, fixed menu, fixed borders */
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    /* Calculate exact window size based on the game's layout margins */
    rc.left = 0; 
    rc.top = 0;
    rc.right = X_MARGIN + (7 * X_SPACING);
    rc.bottom = DEFAULT_TAB_BOTTOM;
    
    /* Adjust for title bar and menu height */
    AdjustWindowRect(&rc, style, TRUE);

    wc.lpfnWndProc   = lpfnWndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = SOL_WC_NAME;
    wc.lpszMenuName  = MAKEINTRESOURCEW(IDR_MAINMENU);
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_SOLITAIRE));
    wc.hbrBackground = CreateSolidBrush(SOL_BG_COLOR);

    if (!RegisterClassW(&wc)) return NULL;

    hwnd = CreateWindowW(SOL_WC_NAME, L"Solitaire", style,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         rc.right - rc.left, rc.bottom - rc.top,
                         NULL, NULL, hInst, NULL);

    if (hwnd) {
        ShowWindow(hwnd, nCmdShow);
        UpdateWindow(hwnd);
    }

    return hwnd;
}

void Res_GetMinMaxInfo(LPARAM lp)
{
    MINMAXINFO *mmi = (MINMAXINFO*)lp;
    mmi->ptMinTrackSize.x = MIN_WIDTH;
    mmi->ptMinTrackSize.y = MIN_HEIGHT;
}