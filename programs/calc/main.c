/*
 * Moonshine Calculator
 * Windows XP/Vista/7 classic style
 * Written natively for Wine/Moonshine
 */

#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define MAX_DISPLAY 32

/* Calculator state */
static double current = 0.0;
static double operand = 0.0;
static double memory  = 0.0;
static int    op      = 0;
static BOOL   new_input   = TRUE;
static BOOL   has_operand = FALSE;
static BOOL   error       = FALSE;
static BOOL   sci_mode    = FALSE;
static WCHAR  display_str[MAX_DISPLAY] = L"0";

/* Button IDs */
#define ID_0        100
#define ID_1        101
#define ID_2        102
#define ID_3        103
#define ID_4        104
#define ID_5        105
#define ID_6        106
#define ID_7        107
#define ID_8        108
#define ID_9        109
#define ID_DOT      110
#define ID_ADD      111
#define ID_SUB      112
#define ID_MUL      113
#define ID_DIV      114
#define ID_EQ       115
#define ID_CLR      116
#define ID_CE       117
#define ID_BACK     118
#define ID_SIGN     119
#define ID_SQRT     120
#define ID_PERCENT  121
#define ID_RECIP    122
#define ID_MC       123
#define ID_MR       124
#define ID_MS       125
#define ID_MPLUS    126
#define ID_MMINUS   127
#define ID_DISPLAY  200

#define OP_NONE  0
#define OP_ADD   1
#define OP_SUB   2
#define OP_MUL   3
#define OP_DIV   4

static HWND hwnd_display;

static void update_display(HWND hwnd)
{
    SetWindowTextW(GetDlgItem(hwnd, ID_DISPLAY), display_str);
}

static void set_display(double val)
{
    /* Format like classic Windows calculator */
    if (val == (long long)val && fabs(val) < 1e15)
        swprintf(display_str, MAX_DISPLAY, L"%.0f", val);
    else
        swprintf(display_str, MAX_DISPLAY, L"%.10g", val);
}

static void calc_error(HWND hwnd)
{
    error = TRUE;
    wcscpy(display_str, L"0");
    update_display(hwnd);
}

static void do_equals(HWND hwnd)
{
    double input;

    if (error) return;
    if (!has_operand) return;

    input = _wtof(display_str);

    switch (op)
    {
    case OP_ADD: current = operand + input; break;
    case OP_SUB: current = operand - input; break;
    case OP_MUL: current = operand * input; break;
    case OP_DIV:
        if (input == 0.0) { wcscpy(display_str, L"Cannot divide by zero"); error = TRUE; update_display(hwnd); return; }
        current = operand / input;
        break;
    default: current = input; break;
    }

    set_display(current);
    update_display(hwnd);
    has_operand = FALSE;
    new_input   = TRUE;
    op          = OP_NONE;
}

static void handle_op(HWND hwnd, int new_op)
{
    if (error) return;

    if (has_operand)
        do_equals(hwnd);

    operand     = _wtof(display_str);
    op          = new_op;
    has_operand = TRUE;
    new_input   = TRUE;
}

static void handle_digit(HWND hwnd, WCHAR ch)
{
    if (error) return;

    if (new_input)
    {
        display_str[0] = ch;
        display_str[1] = L'\0';
        new_input = FALSE;
    }
    else
    {
        /* Don't allow more digits than display */
        if (wcslen(display_str) >= MAX_DISPLAY - 1) return;
        /* Don't allow leading zeros */
        if (wcscmp(display_str, L"0") == 0 && ch != L'.')
        {
            display_str[0] = ch;
            display_str[1] = L'\0';
        }
        else
        {
            WCHAR tmp[2] = {ch, 0};
            wcscat(display_str, tmp);
        }
    }
    update_display(hwnd);
}

static void handle_dot(HWND hwnd)
{
    if (error) return;

    if (new_input)
    {
        wcscpy(display_str, L"0.");
        new_input = FALSE;
    }
    else if (!wcschr(display_str, L'.'))
    {
        wcscat(display_str, L".");
    }
    update_display(hwnd);
}

static void handle_back(HWND hwnd)
{
    int len;

    if (error || new_input) return;

    len = wcslen(display_str);
    if (len <= 1)
    {
        wcscpy(display_str, L"0");
        new_input = TRUE;
    }
    else
    {
        display_str[len - 1] = L'\0';
        /* If only minus sign left */
        if (wcscmp(display_str, L"-") == 0)
        {
            wcscpy(display_str, L"0");
            new_input = TRUE;
        }
    }
    update_display(hwnd);
}

static LRESULT CALLBACK CalcWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_COMMAND:
    {
        int id = LOWORD(wp);

        switch (id)
        {
        /* Digits */
        case ID_0: handle_digit(hwnd, L'0'); break;
        case ID_1: handle_digit(hwnd, L'1'); break;
        case ID_2: handle_digit(hwnd, L'2'); break;
        case ID_3: handle_digit(hwnd, L'3'); break;
        case ID_4: handle_digit(hwnd, L'4'); break;
        case ID_5: handle_digit(hwnd, L'5'); break;
        case ID_6: handle_digit(hwnd, L'6'); break;
        case ID_7: handle_digit(hwnd, L'7'); break;
        case ID_8: handle_digit(hwnd, L'8'); break;
        case ID_9: handle_digit(hwnd, L'9'); break;
        case ID_DOT: handle_dot(hwnd); break;

        /* Operators */
        case ID_ADD: handle_op(hwnd, OP_ADD); break;
        case ID_SUB: handle_op(hwnd, OP_SUB); break;
        case ID_MUL: handle_op(hwnd, OP_MUL); break;
        case ID_DIV: handle_op(hwnd, OP_DIV); break;
        case ID_EQ:  do_equals(hwnd); break;

        /* Clear */
        case ID_CLR:
            current = operand = 0.0;
            op = OP_NONE;
            has_operand = FALSE;
            new_input   = TRUE;
            error       = FALSE;
            wcscpy(display_str, L"0");
            update_display(hwnd);
            break;

        case ID_CE:
            error = FALSE;
            wcscpy(display_str, L"0");
            new_input = TRUE;
            update_display(hwnd);
            break;

        case ID_BACK:
            handle_back(hwnd);
            break;

        /* Unary ops */
        case ID_SIGN:
            if (!error)
            {
                double v = _wtof(display_str);
                v = -v;
                set_display(v);
                update_display(hwnd);
            }
            break;

        case ID_SQRT:
            if (!error)
            {
                double v = _wtof(display_str);
                if (v < 0) { wcscpy(display_str, L"Invalid input"); error = TRUE; update_display(hwnd); }
                else { set_display(sqrt(v)); update_display(hwnd); new_input = TRUE; }
            }
            break;

        case ID_PERCENT:
            if (!error && has_operand)
            {
                double v = _wtof(display_str);
                set_display(operand * v / 100.0);
                update_display(hwnd);
                new_input = TRUE;
            }
            break;

        case ID_RECIP:
            if (!error)
            {
                double v = _wtof(display_str);
                if (v == 0) { wcscpy(display_str, L"Cannot divide by zero"); error = TRUE; update_display(hwnd); }
                else { set_display(1.0 / v); update_display(hwnd); new_input = TRUE; }
            }
            break;

        /* Memory */
        case ID_MC: memory = 0.0; break;
        case ID_MR: if (!error) { set_display(memory); update_display(hwnd); new_input = TRUE; } break;
        case ID_MS: if (!error) { memory = _wtof(display_str); new_input = TRUE; } break;
        case ID_MPLUS: if (!error) memory += _wtof(display_str); break;
        case ID_MMINUS: if (!error) memory -= _wtof(display_str); break;
        }
        break;
    }

    case WM_KEYDOWN:
        switch (wp)
        {
        case VK_ESCAPE: SendMessageW(hwnd, WM_COMMAND, ID_CLR, 0); break;
        case VK_BACK:   SendMessageW(hwnd, WM_COMMAND, ID_BACK, 0); break;
        case VK_RETURN: SendMessageW(hwnd, WM_COMMAND, ID_EQ, 0); break;
        }
        break;

    case WM_CHAR:
        switch ((WCHAR)wp)
        {
        case L'0': case L'1': case L'2': case L'3': case L'4':
        case L'5': case L'6': case L'7': case L'8': case L'9':
            handle_digit(hwnd, (WCHAR)wp); break;
        case L'.': case L',': handle_dot(hwnd); break;
        case L'+': handle_op(hwnd, OP_ADD); break;
        case L'-': handle_op(hwnd, OP_SUB); break;
        case L'*': handle_op(hwnd, OP_MUL); break;
        case L'/': handle_op(hwnd, OP_DIV); break;
        case L'=': do_equals(hwnd); break;
        case L'%': SendMessageW(hwnd, WM_COMMAND, ID_PERCENT, 0); break;
        case L'r': case L'R': SendMessageW(hwnd, WM_COMMAND, ID_RECIP, 0); break;
        case L'@': SendMessageW(hwnd, WM_COMMAND, ID_SQRT, 0); break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

static HWND make_button(HWND parent, const WCHAR *label, int id, int x, int y, int w, int h)
{
    return CreateWindowW(L"BUTTON", label,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, w, h,
        parent, (HMENU)(INT_PTR)id,
        (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE), NULL);
}

static void create_controls(HWND hwnd)
{
    /* Display */
    CreateWindowW(L"STATIC", L"0",
        WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_SUNKEN,
        6, 6, 258, 25,
        hwnd, (HMENU)ID_DISPLAY, 
        (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE), NULL);

    #define BW 60  /* button width */
    #define BH 26  /* button height */
    #define BG 4   /* gap */
    #define X0 6   /* left margin */
    #define Y0 36  /* top of buttons */

    /* Row 0: MC MR MS M+ M- */
    make_button(hwnd, L"MC",   ID_MC,     X0,                Y0,           BW-4, BH);
    make_button(hwnd, L"MR",   ID_MR,     X0+(BW),           Y0,           BW-4, BH);
    make_button(hwnd, L"MS",   ID_MS,     X0+(BW*2),         Y0,           BW-4, BH);
    make_button(hwnd, L"M+",   ID_MPLUS,  X0+(BW*3),         Y0,           BW-4, BH);
    make_button(hwnd, L"M-",   ID_MMINUS, X0+(BW*4)-BG,      Y0,           BW-4, BH);

    /* Row 1: <- CE C +/- sqrt */
    make_button(hwnd, L"\u2190", ID_BACK,  X0,                Y0+(BH+BG),   BW-4, BH);
    make_button(hwnd, L"CE",   ID_CE,     X0+(BW),           Y0+(BH+BG),   BW-4, BH);
    make_button(hwnd, L"C",    ID_CLR,    X0+(BW*2),         Y0+(BH+BG),   BW-4, BH);
    make_button(hwnd, L"\u00b1", ID_SIGN, X0+(BW*3),         Y0+(BH+BG),   BW-4, BH);
    make_button(hwnd, L"\u221a", ID_SQRT, X0+(BW*4)-BG,      Y0+(BH+BG),   BW-4, BH);

    /* Row 2: 7 8 9 / % */
    make_button(hwnd, L"7",    ID_7,      X0,                Y0+(BH+BG)*2, BW-4, BH);
    make_button(hwnd, L"8",    ID_8,      X0+(BW),           Y0+(BH+BG)*2, BW-4, BH);
    make_button(hwnd, L"9",    ID_9,      X0+(BW*2),         Y0+(BH+BG)*2, BW-4, BH);
    make_button(hwnd, L"/",    ID_DIV,    X0+(BW*3),         Y0+(BH+BG)*2, BW-4, BH);
    make_button(hwnd, L"%",    ID_PERCENT,X0+(BW*4)-BG,      Y0+(BH+BG)*2, BW-4, BH);

    /* Row 3: 4 5 6 * 1/x */
    make_button(hwnd, L"4",    ID_4,      X0,                Y0+(BH+BG)*3, BW-4, BH);
    make_button(hwnd, L"5",    ID_5,      X0+(BW),           Y0+(BH+BG)*3, BW-4, BH);
    make_button(hwnd, L"6",    ID_6,      X0+(BW*2),         Y0+(BH+BG)*3, BW-4, BH);
    make_button(hwnd, L"*",    ID_MUL,    X0+(BW*3),         Y0+(BH+BG)*3, BW-4, BH);
    make_button(hwnd, L"1/x",  ID_RECIP,  X0+(BW*4)-BG,      Y0+(BH+BG)*3, BW-4, BH);

    /* Row 4: 1 2 3 - = (tall) */
    make_button(hwnd, L"1",    ID_1,      X0,                Y0+(BH+BG)*4, BW-4, BH);
    make_button(hwnd, L"2",    ID_2,      X0+(BW),           Y0+(BH+BG)*4, BW-4, BH);
    make_button(hwnd, L"3",    ID_3,      X0+(BW*2),         Y0+(BH+BG)*4, BW-4, BH);
    make_button(hwnd, L"-",    ID_SUB,    X0+(BW*3),         Y0+(BH+BG)*4, BW-4, BH);
    /* = spans rows 4 and 5 */
    make_button(hwnd, L"=",    ID_EQ,     X0+(BW*4)-BG,      Y0+(BH+BG)*4, BW-4, (BH*2)+BG);

    /* Row 5: 0 (wide) . + */
    make_button(hwnd, L"0",    ID_0,      X0,                Y0+(BH+BG)*5, (BW*2)-4, BH);
    make_button(hwnd, L".",    ID_DOT,    X0+(BW*2),         Y0+(BH+BG)*5, BW-4, BH);
    make_button(hwnd, L"+",    ID_ADD,    X0+(BW*3),         Y0+(BH+BG)*5, BW-4, BH);

    #undef BW
    #undef BH
    #undef BG
    #undef X0
    #undef Y0
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPWSTR cmdline, int cmdshow)
{
    WNDCLASSW wc = {0};
    HWND hwnd;
    MSG msg;

    wc.lpfnWndProc   = CalcWndProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"CalcWnd";
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon         = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    RegisterClassW(&wc);

    hwnd = CreateWindowW(L"CalcWnd", L"Calculator",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        276, 226,
        NULL, NULL, hInstance, NULL);

    create_controls(hwnd);
    ShowWindow(hwnd, cmdshow);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return msg.wParam;
}