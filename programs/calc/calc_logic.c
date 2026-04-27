#include "calc_logic.h"
#include <math.h>
#include <stdio.h>
#include <wchar.h>

/* State Definitions */
double current = 0.0;
double operand = 0.0;
double memory  = 0.0;
int    op      = OP_NONE;
BOOL   new_input   = TRUE;
BOOL   has_operand = FALSE;
BOOL   error       = FALSE;
WCHAR  display_str[MAX_DISPLAY] = L"0";

static void update_ui_text(HWND hwnd) {
    SetWindowTextW(GetDlgItem(hwnd, ID_DISPLAY), display_str);
}

void set_display(double val) {
    if (val == (long long)val && fabs(val) < 1e15)
        swprintf(display_str, MAX_DISPLAY, L"%.0f", val);
    else
        swprintf(display_str, MAX_DISPLAY, L"%.10g", val);
}

void calc_error(HWND hwnd, const WCHAR* msg) {
    error = TRUE;
    wcscpy(display_str, msg);
    update_ui_text(hwnd);
}

void do_equals(HWND hwnd) {
    double input;
    if (error) return;
    if (!has_operand) return;

    input = _wtof(display_str);

    switch (op) {
        case OP_ADD: current = operand + input; break;
        case OP_SUB: current = operand - input; break;
        case OP_MUL: current = operand * input; break;
        case OP_DIV:
            if (input == 0.0) {
                calc_error(hwnd, L"Cannot divide by zero");
                return;
            }
            current = operand / input;
            break;
        default: current = input; break;
    }

    set_display(current);
    update_ui_text(hwnd);
    has_operand = FALSE;
    new_input   = TRUE;
    op          = OP_NONE;
}

void handle_op(HWND hwnd, int new_op) {
    if (error) return;
    if (has_operand) do_equals(hwnd);

    operand     = _wtof(display_str);
    op          = new_op;
    has_operand = TRUE;
    new_input   = TRUE;
}

void handle_digit(HWND hwnd, WCHAR ch) {
    if (error) return;

    if (new_input) {
        display_str[0] = ch;
        display_str[1] = L'\0';
        new_input = FALSE;
    } else {
        if (wcslen(display_str) >= MAX_DISPLAY - 1) return;
        if (wcscmp(display_str, L"0") == 0 && ch != L'.') {
            display_str[0] = ch;
            display_str[1] = L'\0';
        } else {
            WCHAR tmp[2] = {ch, 0};
            wcscat(display_str, tmp);
        }
    }
    update_ui_text(hwnd);
}

void handle_dot(HWND hwnd) {
    if (error) return;
    if (new_input) {
        wcscpy(display_str, L"0.");
        new_input = FALSE;
    } else if (!wcschr(display_str, L'.')) {
        wcscat(display_str, L".");
    }
    update_ui_text(hwnd);
}

void handle_back(HWND hwnd) {
    int len;
    if (error || new_input) return;

    len = wcslen(display_str);
    if (len <= 1) {
        wcscpy(display_str, L"0");
        new_input = TRUE;
    } else {
        display_str[len - 1] = L'\0';
        if (wcscmp(display_str, L"-") == 0) {
            wcscpy(display_str, L"0");
            new_input = TRUE;
        }
    }
    update_ui_text(hwnd);
}