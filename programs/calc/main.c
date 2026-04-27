#include <windows.h>
#include <commctrl.h>
#include "calc.h"
#include "calc_logic.h"

static HWND make_button(HWND parent, const WCHAR *label, int id, int x, int y, int w, int h) {
    return CreateWindowW(L"BUTTON", label,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, w, h,
        parent, (HMENU)(INT_PTR)id,
        (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE), NULL);
}

static void create_controls(HWND hwnd) {
    CreateWindowW(L"STATIC", L"0",
        WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_SUNKEN,
        6, 6, 258, 25,
        hwnd, (HMENU)ID_DISPLAY, 
        (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE), NULL);

    #define BW 60 
    #define BH 26 
    #define BG 4  
    #define X0 6  
    #define Y0 36 

    make_button(hwnd, L"MC",   ID_MC,     X0,                Y0,           BW-4, BH);
    make_button(hwnd, L"MR",   ID_MR,     X0+(BW),           Y0,           BW-4, BH);
    make_button(hwnd, L"MS",   ID_MS,     X0+(BW*2),         Y0,           BW-4, BH);
    make_button(hwnd, L"M+",   ID_MPLUS,  X0+(BW*3),         Y0,           BW-4, BH);
    make_button(hwnd, L"M-",   ID_MMINUS, X0+(BW*4)-BG,      Y0,           BW-4, BH);

    make_button(hwnd, L"\u2190", ID_BACK,  X0,                Y0+(BH+BG),   BW-4, BH);
    make_button(hwnd, L"CE",   ID_CE,     X0+(BW),           Y0+(BH+BG),   BW-4, BH);
    make_button(hwnd, L"C",    ID_CLR,    X0+(BW*2),         Y0+(BH+BG),   BW-4, BH);
    make_button(hwnd, L"\u00b1", ID_SIGN, X0+(BW*3),         Y0+(BH+BG),   BW-4, BH);
    make_button(hwnd, L"\u221a", ID_SQRT, X0+(BW*4)-BG,      Y0+(BH+BG),   BW-4, BH);

    make_button(hwnd, L"7",    ID_7,      X0,                Y0+(BH+BG)*2, BW-4, BH);
    make_button(hwnd, L"8",    ID_8,      X0+(BW),           Y0+(BH+BG)*2, BW-4, BH);
    make_button(hwnd, L"9",    ID_9,      X0+(BW*2),         Y0+(BH+BG)*2, BW-4, BH);
    make_button(hwnd, L"/",    ID_DIV,    X0+(BW*3),         Y0+(BH+BG)*2, BW-4, BH);
    make_button(hwnd, L"%",    ID_PERCENT,X0+(BW*4)-BG,      Y0+(BH+BG)*2, BW-4, BH);

    make_button(hwnd, L"4",    ID_4,      X0,                Y0+(BH+BG)*3, BW-4, BH);
    make_button(hwnd, L"5",    ID_5,      X0+(BW),           Y0+(BH+BG)*3, BW-4, BH);
    make_button(hwnd, L"6",    ID_6,      X0+(BW*2),         Y0+(BH+BG)*3, BW-4, BH);
    make_button(hwnd, L"*",    ID_MUL,    X0+(BW*3),         Y0+(BH+BG)*3, BW-4, BH);
    make_button(hwnd, L"1/x",  ID_RECIP,  X0+(BW*4)-BG,      Y0+(BH+BG)*3, BW-4, BH);

    make_button(hwnd, L"1",    ID_1,      X0,                Y0+(BH+BG)*4, BW-4, BH);
    make_button(hwnd, L"2",    ID_2,      X0+(BW),           Y0+(BH+BG)*4, BW-4, BH);
    make_button(hwnd, L"3",    ID_3,      X0+(BW*2),         Y0+(BH+BG)*4, BW-4, BH);
    make_button(hwnd, L"-",    ID_SUB,    X0+(BW*3),         Y0+(BH+BG)*4, BW-4, BH);
    make_button(hwnd, L"=",    ID_EQ,     X0+(BW*4)-BG,      Y0+(BH+BG)*4, BW-4, (BH*2)+BG);

    make_button(hwnd, L"0",    ID_0,      X0,                Y0+(BH+BG)*5, (BW*2)-4, BH);
    make_button(hwnd, L".",    ID_DOT,    X0+(BW*2),         Y0+(BH+BG)*5, BW-4, BH);
    make_button(hwnd, L"+",    ID_ADD,    X0+(BW*3),         Y0+(BH+BG)*5, BW-4, BH);
}

static LRESULT CALLBACK CalcWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
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
                    wcscpy(display_str, L"0"); SetWindowTextW(GetDlgItem(hwnd, ID_DISPLAY), display_str);
                    break;
                case ID_CE:
                    error = FALSE; wcscpy(display_str, L"0"); new_input = TRUE;
                    SetWindowTextW(GetDlgItem(hwnd, ID_DISPLAY), display_str);
                    break;
                case ID_BACK: handle_back(hwnd); break;
                case ID_SIGN:
                    if (!error) { set_display(-_wtof(display_str)); SetWindowTextW(GetDlgItem(hwnd, ID_DISPLAY), display_str); }
                    break;
                case ID_SQRT:
                    if (!error) {
                        double v = _wtof(display_str);
                        if (v < 0) calc_error(hwnd, L"Invalid input");
                        else { set_display(sqrt(v)); SetWindowTextW(GetDlgItem(hwnd, ID_DISPLAY), display_str); new_input = TRUE; }
                    }
                    break;
                case ID_MC: memory = 0.0; break;
                case ID_MR: if (!error) { set_display(memory); SetWindowTextW(GetDlgItem(hwnd, ID_DISPLAY), display_str); new_input = TRUE; } break;
                case ID_MS: if (!error) { memory = _wtof(display_str); new_input = TRUE; } break;
                case ID_MPLUS: if (!error) memory += _wtof(display_str); break;
                case ID_MMINUS: if (!error) memory -= _wtof(display_str); break;
            }
            break;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) SendMessageW(hwnd, WM_COMMAND, ID_CLR, 0);
            if (wp == VK_BACK)   SendMessageW(hwnd, WM_COMMAND, ID_BACK, 0);
            if (wp == VK_RETURN) SendMessageW(hwnd, WM_COMMAND, ID_EQ, 0);
            break;
        case WM_CHAR:
            switch ((WCHAR)wp) {
                case L'0': case L'1': case L'2': case L'3': case L'4':
                case L'5': case L'6': case L'7': case L'8': case L'9':
                    handle_digit(hwnd, (WCHAR)wp); break;
                case L'.': case L',': handle_dot(hwnd); break;
                case L'+': handle_op(hwnd, OP_ADD); break;
                case L'-': handle_op(hwnd, OP_SUB); break;
                case L'*': handle_op(hwnd, OP_MUL); break;
                case L'/': handle_op(hwnd, OP_DIV); break;
                case L'=': do_equals(hwnd); break;
            }
            break;
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
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"CalcWnd", L"Moonshine Calculator",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 276, 226, NULL, NULL, hInstance, NULL);

    create_controls(hwnd);
    ShowWindow(hwnd, cmdshow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return msg.wParam;
}