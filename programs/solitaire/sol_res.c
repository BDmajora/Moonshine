#include "solitaire.h"
#include "sol_res.h"

// Initializes and displays the main game window
HWND Res_InitWindow(HINSTANCE hInst, int nCmdShow, WNDPROC lpfnWndProc)
{
    WNDCLASSW wc = {0};
    HWND hwnd;
    RECT rc;
    
    // Define window style without resizing
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    // Calculate client area based on layout spacing
    rc.left = 0; 
    rc.top = 0;
    rc.right = X_MARGIN + (7 * X_SPACING);
    rc.bottom = DEFAULT_TAB_BOTTOM;
    
    // Adjust rect to include borders and menu
    AdjustWindowRect(&rc, style, TRUE);

    // Set window class parameters
    wc.lpfnWndProc   = lpfnWndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = SOL_WC_NAME;
    wc.lpszMenuName  = MAKEINTRESOURCEW(IDR_MAINMENU);
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_SOLITAIRE));
    wc.hbrBackground = (HBRUSH)(COLOR_DESKTOP + 1); // Use system color to avoid GDI leak

    if (!RegisterClassW(&wc)) return NULL;

    // Create the window instance
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

// Sets minimum window dimensions for WM_GETMINMAXINFO
void Res_GetMinMaxInfo(LPARAM lp)
{
    MINMAXINFO *mmi = (MINMAXINFO*)lp;
    mmi->ptMinTrackSize.x = MIN_WIDTH;
    mmi->ptMinTrackSize.y = MIN_HEIGHT;
}