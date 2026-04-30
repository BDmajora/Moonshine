#include "solitaire.h"

static int cardW, cardH;
static UINT timer_id;

LRESULT CALLBACK SolWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps; HDC hdc; RECT rc; MINMAXINFO *mmi;

    switch(msg) {
    case WM_CREATE:
        if(!cdtInit(&cardW, &cardH)) {
            MessageBoxW(hwnd, L"Failed to load cards.dll", L"Error", MB_ICONERROR);
            return -1;
        }
        Game_Init();
        timer_id = SetTimer(hwnd, 1, 1000, NULL);
        break;

    case WM_TIMER:
        OnTimer(hwnd);
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        hdc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &rc);
        
        /* Double buffering or direct fill */
        { 
            HBRUSH hbr = CreateSolidBrush(SOL_BG_COLOR);
            FillRect(hdc, &rc, hbr); 
            DeleteObject(hbr); 
        }
        
        DrawBoard(hdc, rc.right, rc.bottom);
        EndPaint(hwnd, &ps);
        break;

    case WM_LBUTTONDOWN:
        OnLButtonDown(hwnd, (short)LOWORD(lp), (short)HIWORD(lp));
        break;

    case WM_MOUSEMOVE:
        OnMouseMove(hwnd, (short)LOWORD(lp), (short)HIWORD(lp));
        break;

    case WM_LBUTTONUP:
        OnLButtonUp(hwnd, (short)LOWORD(lp), (short)HIWORD(lp));
        break;

    case WM_GETMINMAXINFO:
        mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = 560;
        mmi->ptMinTrackSize.y = 400;
        break;

    case WM_COMMAND:
        switch(LOWORD(wp)) {
            case IDM_GAME_EXIT: 
                PostQuitMessage(0); 
                break;
            case IDM_GAME_DEAL:
                Game_Init();
                InvalidateRect(hwnd, NULL, TRUE);
                break;
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, timer_id);
        cdtTerm();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR cmd, int show)
{
    WNDCLASSW wc = {0};
    HWND hwnd; MSG msg;
    RECT rc; DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    /* Calculate window size based on layout constants */
    rc.left = 0; rc.top = 0;
    rc.right = X_MARGIN * 2 + 7 * X_SPACING; 
    rc.bottom = Y_TABLEAU + 5 * 96 + STATUS_BAR_HEIGHT + 8;
    AdjustWindowRect(&rc, style, TRUE);

    wc.lpfnWndProc   = SolWndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"SolitaireWnd";
    wc.lpszMenuName  = MAKEINTRESOURCEW(IDR_MAINMENU);
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_SOLITAIRE));
    RegisterClassW(&wc);

    hwnd = CreateWindowW(L"SolitaireWnd", L"Solitaire", style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    while(GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}