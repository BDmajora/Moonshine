#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include "calc.h"
#include "calc_logic.h"

/* Helper to move/resize controls easily */
void move_ctrl(HWND hwnd, int id, int x, int y, int w, int h) {
    HWND ctrl = GetDlgItem(hwnd, id);
    if (ctrl) MoveWindow(ctrl, x, y, w, h, TRUE);
}

/* Helper to update the text box with the current display_str */
void update_display(HWND hwnd) {
    SetWindowTextW(GetDlgItem(hwnd, ID_DISPLAY), display_str);
}

/* Dynamic Layout Engine: Calculates positions based on current window size */
void update_layout(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int win_w = rc.right;
    int win_h = rc.bottom;

    /* Proportions: Matching your reference layout */
    int margin = (int)(win_w * 0.02); 
    int gap    = (int)(win_w * 0.01); 
    
    int display_h = (int)(win_h * 0.15); 
    move_ctrl(hwnd, ID_DISPLAY, margin, margin, win_w - (margin * 2), display_h);

    int x_start = margin;
    int y_start = display_h + (margin * 2);
    int grid_w  = win_w - (margin * 2);
    int grid_h  = win_h - y_start - margin;

    int bw = (grid_w - (gap * 4)) / 5;
    int bh = (grid_h - (gap * 5)) / 6;

    /* Row 0: Memory */
    move_ctrl(hwnd, ID_MC,     x_start,          y_start, bw, bh);
    move_ctrl(hwnd, ID_MR,     x_start+(bw+gap), y_start, bw, bh);
    move_ctrl(hwnd, ID_MS,     x_start+(bw+gap)*2, y_start, bw, bh);
    move_ctrl(hwnd, ID_MPLUS,  x_start+(bw+gap)*3, y_start, bw, bh);
    move_ctrl(hwnd, ID_MMINUS, x_start+(bw+gap)*4, y_start, bw, bh);

    /* Row 1: Operations */
    int r1y = y_start + (bh+gap);
    move_ctrl(hwnd, ID_BACK,   x_start,          r1y, bw, bh);
    move_ctrl(hwnd, ID_CE,     x_start+(bw+gap), r1y, bw, bh);
    move_ctrl(hwnd, ID_CLR,    x_start+(bw+gap)*2, r1y, bw, bh);
    move_ctrl(hwnd, ID_SIGN,   x_start+(bw+gap)*3, r1y, bw, bh);
    move_ctrl(hwnd, ID_SQRT,   x_start+(bw+gap)*4, r1y, bw, bh);

    /* Row 2: 7-9 */
    int r2y = y_start + (bh+gap)*2;
    move_ctrl(hwnd, ID_7,      x_start,          r2y, bw, bh);
    move_ctrl(hwnd, ID_8,      x_start+(bw+gap), r2y, bw, bh);
    move_ctrl(hwnd, ID_9,      x_start+(bw+gap)*2, r2y, bw, bh);
    move_ctrl(hwnd, ID_DIV,    x_start+(bw+gap)*3, r2y, bw, bh);
    move_ctrl(hwnd, ID_PERCENT,x_start+(bw+gap)*4, r2y, bw, bh);

    /* Row 3: 4-6 */
    int r3y = y_start + (bh+gap)*3;
    move_ctrl(hwnd, ID_4,      x_start,          r3y, bw, bh);
    move_ctrl(hwnd, ID_5,      x_start+(bw+gap), r3y, bw, bh);
    move_ctrl(hwnd, ID_6,      x_start+(bw+gap)*2, r3y, bw, bh);
    move_ctrl(hwnd, ID_MUL,    x_start+(bw+gap)*3, r3y, bw, bh);
    move_ctrl(hwnd, ID_RECIP,  x_start+(bw+gap)*4, r3y, bw, bh);

    /* Row 4: 1-3 + Equals */
    int r4y = y_start + (bh+gap)*4;
    move_ctrl(hwnd, ID_1,      x_start,          r4y, bw, bh);
    move_ctrl(hwnd, ID_2,      x_start+(bw+gap), r4y, bw, bh);
    move_ctrl(hwnd, ID_3,      x_start+(bw+gap)*2, r4y, bw, bh);
    move_ctrl(hwnd, ID_SUB,    x_start+(bw+gap)*3, r4y, bw, bh);
    move_ctrl(hwnd, ID_EQ,     x_start+(bw+gap)*4, r4y, bw, (bh*2)+gap);

    /* Row 5: 0, Dot, Add */
    int r5y = y_start + (bh+gap)*5;
    move_ctrl(hwnd, ID_0,      x_start,          r5y, (bw*2)+gap, bh);
    move_ctrl(hwnd, ID_DOT,    x_start+(bw+gap)*2, r5y, bw, bh);
    move_ctrl(hwnd, ID_ADD,    x_start+(bw+gap)*3, r5y, bw, bh);
}

static void create_controls(HWND hwnd) {
    CreateWindowW(L"STATIC", L"0", WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_SUNKEN, 
                 0, 0, 0, 0, hwnd, (HMENU)ID_DISPLAY, NULL, NULL);

    struct { int id; const WCHAR* lbl; } btns[] = {
        {ID_MC, L"MC"}, {ID_MR, L"MR"}, {ID_MS, L"MS"}, {ID_MPLUS, L"M+"}, {ID_MMINUS, L"M-"},
        {ID_BACK, L"\u2190"}, {ID_CE, L"CE"}, {ID_CLR, L"C"}, {ID_SIGN, L"\u00b1"}, {ID_SQRT, L"\u221a"},
        {ID_7, L"7"}, {ID_8, L"8"}, {ID_9, L"9"}, {ID_DIV, L"/"}, {ID_PERCENT, L"%"},
        {ID_4, L"4"}, {ID_5, L"5"}, {ID_6, L"6"}, {ID_MUL, L"*"}, {ID_RECIP, L"1/x"},
        {ID_1, L"1"}, {ID_2, L"2"}, {ID_3, L"3"}, {ID_SUB, L"-"}, {ID_EQ, L"="},
        {ID_0, L"0"}, {ID_DOT, L"."}, {ID_ADD, L"+"}
    };

    for(int i=0; i < (int)(sizeof(btns)/sizeof(btns[0])); i++) {
        CreateWindowW(L"BUTTON", btns[i].lbl, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)btns[i].id, NULL, NULL);
    }
}

static LRESULT CALLBACK CalcWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            create_controls(hwnd);
            break;
        case WM_SIZE:
            update_layout(hwnd);
            break;
        case WM_COMMAND: {
            int id = LOWORD(wp);
            switch (id) {
                case ID_0: case ID_1: case ID_2: case ID_3: case ID_4:
                case ID_5: case ID_6: case ID_7: case ID_8: case ID_9:
                    handle_digit(hwnd, (WCHAR)(L'0' + (id - ID_0))); break;
                case ID_DOT: handle_dot(hwnd); break;
                case ID_ADD: handle_op(hwnd, OP_ADD); break;
                case ID_SUB: handle_op(hwnd, OP_SUB); break;
                case ID_MUL: handle_op(hwnd, OP_MUL); break;
                case ID_DIV: handle_op(hwnd, OP_DIV); break;
                case ID_EQ:  do_equals(hwnd); break;
                case ID_CLR:
                    current = operand = 0.0; op = OP_NONE;
                    has_operand = FALSE; new_input = TRUE; error = FALSE;
                    wcscpy(display_str, L"0"); update_display(hwnd);
                    break;
                case ID_CE:
                    error = FALSE; wcscpy(display_str, L"0"); new_input = TRUE;
                    update_display(hwnd);
                    break;
                case ID_BACK: handle_back(hwnd); break;
                case ID_SIGN:
                    if (!error) { set_display(-_wtof(display_str)); update_display(hwnd); }
                    break;
                case ID_SQRT:
                    if (!error) {
                        double v = _wtof(display_str);
                        if (v < 0) { wcscpy(display_str, L"Invalid input"); error = TRUE; update_display(hwnd); }
                        else { set_display(sqrt(v)); update_display(hwnd); new_input = TRUE; }
                    }
                    break;
                case ID_MC: memory = 0.0; break;
                case ID_MR: if (!error) { set_display(memory); update_display(hwnd); new_input = TRUE; } break;
                case ID_MS: if (!error) { memory = _wtof(display_str); new_input = TRUE; } break;
                case ID_MPLUS: if (!error) memory += _wtof(display_str); break;
                case ID_MMINUS: if (!error) memory -= _wtof(display_str); break;
            }
            break;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO *)lp;
            mmi->ptMinTrackSize.x = 260;
            mmi->ptMinTrackSize.y = 320;
            break;
        }
        case WM_DESTROY: PostQuitMessage(0); break;
        default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPWSTR cmdline, int cmdshow) {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = CalcWndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"CalcWnd";
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon = LoadIconW(hInstance, (LPCWSTR)IDI_APPLICATION); 
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"CalcWnd", L"Moonshine Calculator",
        WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, CW_USEDEFAULT, 280, 380, NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, cmdshow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}