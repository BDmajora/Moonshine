#include "calc_ops.h"
#include "calc_memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

/* ── Core operations ─────────────────────────────────────────────── */

static double perform_op(double a, double b, int the_op) {
    switch (the_op) {
        case OP_ADD: return a + b;
        case OP_SUB: return a - b;
        case OP_MUL: return a * b;
        case OP_DIV: return (b != 0.0) ? a / b : 1e308 * 2; /* sentinel */
        case OP_POW: return pow(a, b);
        case OP_MOD: return (b != 0) ? fmod(a, b) : 1e308 * 2;
        case OP_AND: return (double)((long long)a & (long long)b);
        case OP_OR:  return (double)((long long)a | (long long)b);
        case OP_XOR: return (double)((long long)a ^ (long long)b);
        case OP_LSH: return (double)((long long)a << (int)b);
        case OP_RSH: return (double)((long long)a >> (int)b);
        case OP_NROOT: return (b != 0) ? pow(a, 1.0/b) : 0;
        default: return b;
    }
}

void do_equals(HWND hwnd) {
    double input, result;
    WCHAR hist_entry[MAX_DISPLAY * 3];

    if (error) return;
    if (!has_operand) return;

    input = _wtof(display_str);

    if ((op == OP_DIV || op == OP_MOD) && input == 0.0) {
        calc_error(hwnd, L"Cannot divide by zero");
        return;
    }

    result = perform_op(operand, input, op);

    if (!_finite(result)) {
        calc_error(hwnd, L"Overflow");
        return;
    }

    /* Build history string */
    swprintf(hist_entry, MAX_DISPLAY*3, L"%.10g %s %.10g = %.10g",
             operand,
             op==OP_ADD?L"+":op==OP_SUB?L"-":op==OP_MUL?L"*":
             op==OP_DIV?L"/":op==OP_MOD?L"Mod":op==OP_POW?L"^":L"?",
             input, result);
    history_push(hist_entry);

    current     = result;
    set_display(result);
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