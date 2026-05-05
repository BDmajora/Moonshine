#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <wchar.h>
#include "calc.h"
#include "calc_logic.h"
#include "calc_ui.h"

/* ── Scientific unary operations ────────────────────────────────── */

static double calc_to_radians(double v) {
    if (angle_unit == ANGLE_DEG) return v * 3.14159265358979323846 / 180.0;
    if (angle_unit == ANGLE_GRAD) return v * 3.14159265358979323846 / 200.0;
    return v;
}

static double calc_from_radians(double r) {
    if (angle_unit == ANGLE_DEG) return r * 180.0 / 3.14159265358979323846;
    if (angle_unit == ANGLE_GRAD) return r * 200.0 / 3.14159265358979323846;
    return r;
}

void sci_unary(HWND hwnd, int id) {
    double v, r;
    long long iv;
    long long factorial;
    long long fi;
    if (error) return;
    v = _wtof(display_str);
    r = v;

    switch (id) {
        case ID_SCI_SIN:
            r = inv_mode
                ? calc_from_radians(asin(v))
                : sin(calc_to_radians(v));
            break;
        case ID_SCI_COS:
            r = inv_mode
                ? calc_from_radians(acos(v))
                : cos(calc_to_radians(v));
            break;
        case ID_SCI_TAN:
            r = inv_mode
                ? calc_from_radians(atan(v))
                : tan(calc_to_radians(v));
            break;
        case ID_SCI_SINH:
            r = inv_mode ? log(v + sqrt(v*v + 1.0)) : sinh(v);
            break;
        case ID_SCI_COSH:
            r = inv_mode ? log(v + sqrt(v*v - 1.0)) : cosh(v);
            break;
        case ID_SCI_TANH:
            if (inv_mode) {
                if (v <= -1.0 || v >= 1.0) {
                    calc_error(hwnd, L"Invalid input"); return;
                }
                r = 0.5 * log((1.0 + v) / (1.0 - v));
            } else {
                r = tanh(v);
            }
            break;
        case ID_SCI_LOG:
            if (inv_mode) {
                r = pow(10.0, v);
            } else {
                if (v <= 0) { calc_error(hwnd, L"Invalid input"); return; }
                r = log10(v);
            }
            break;
        case ID_SCI_LN:
            if (inv_mode) {
                r = exp(v);
            } else {
                if (v <= 0) { calc_error(hwnd, L"Invalid input"); return; }
                r = log(v);
            }
            break;
        case ID_SCI_POW2:
            r = v * v;
            break;
        case ID_SCI_POW3:
            r = v * v * v;
            break;
        case ID_SCI_CUBE:
            r = cbrt(v);
            break;
        case ID_SCI_FACT:
            iv = (long long)v;
            if (iv < 0 || iv > 20) {
                calc_error(hwnd, L"Invalid input"); return;
            }
            factorial = 1;
            for (fi = 2; fi <= iv; fi++) factorial *= fi;
            r = (double)factorial;
            break;
        case ID_SCI_PI:
            r = 3.14159265358979323846;
            break;
        case ID_SCI_INT:
            r = (double)(long long)v;
            break;
        case ID_SCI_DMS: {
            /* Decimal degrees -> D.MMSS */
            int deg = (int)v;
            int mn  = (int)((v - deg) * 60.0);
            double sec = ((v - deg) * 60.0 - mn) * 60.0;
            r = deg + mn / 100.0 + sec / 10000.0;
            break;
        }
        default:
            return;
    }

    set_display(r);
    ui_update_display(hwnd);
    new_input = TRUE;
}