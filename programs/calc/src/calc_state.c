#include "calc_state.h"
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <math.h>

/* ── State Initialization ────────────────────────────────────────── */
double  current       = 0.0;
double  operand       = 0.0;
double  memory        = 0.0;
int     op            = OP_NONE;
BOOL    new_input     = TRUE;
BOOL    has_operand   = FALSE;
BOOL    error         = FALSE;
BOOL    inv_mode      = FALSE;
BOOL    fe_mode       = FALSE;
BOOL    digit_grouping = FALSE;
int     angle_unit    = ANGLE_DEG;
int     prog_base     = BASE_DEC;
int     prog_word     = WORD_QWORD;
int     paren_depth   = 0;

WCHAR   display_str[MAX_DISPLAY] = L"0";

double  stat_data[MAX_STAT];
int     stat_count = 0;

WCHAR   history[MAX_HIST][MAX_DISPLAY * 3];
int     hist_count = 0;

/* ── Helpers ─────────────────────────────────────────────────────── */

void update_ui_text(HWND hwnd) {
    HWND hd = GetDlgItem(hwnd, ID_DISPLAY);
    if (hd) SetWindowTextW(hd, display_str);
}

void set_display(double val) {
    if (fe_mode) {
        swprintf(display_str, MAX_DISPLAY, L"%.6e", val);
        return;
    }
    if (val == (long long)val && fabs(val) < 1e15)
        swprintf(display_str, MAX_DISPLAY, L"%.0f", val);
    else
        swprintf(display_str, MAX_DISPLAY, L"%.10g", val);
}

void set_display_int(long long val) {
    switch (prog_base) {
        case BASE_HEX:
            swprintf(display_str, MAX_DISPLAY,
                     val < 0 ? L"-%llX" : L"%llX",
                     val < 0 ? -val : val);
            break;
        case BASE_OCT:
            swprintf(display_str, MAX_DISPLAY,
                     val < 0 ? L"-%llo" : L"%llo",
                     val < 0 ? -val : val);
            break;
        case BASE_BIN: {
            unsigned long long uv = (unsigned long long)val;
            int bits, i, pos = 0;
            WCHAR buf[68] = {0};
            switch (prog_word) {
                case WORD_BYTE:  bits = 8;  uv &= 0xFFULL;        break;
                case WORD_WORD:  bits = 16; uv &= 0xFFFFULL;      break;
                case WORD_DWORD: bits = 32; uv &= 0xFFFFFFFFULL;  break;
                default:         bits = 64;                         break;
            }
            for (i = bits - 1; i >= 0; i--)
                buf[pos++] = (uv >> i) & 1 ? L'1' : L'0';
            buf[pos] = L'\0';
            wcscpy(display_str, buf);
            break;
        }
        default:
            swprintf(display_str, MAX_DISPLAY, L"%lld", val);
            break;
    }
}

long long apply_word_mask(long long v) {
    switch (prog_word) {
        case WORD_BYTE:  return (long long)(signed char)(v & 0xFF);
        case WORD_WORD:  return (long long)(short)(v & 0xFFFF);
        case WORD_DWORD: return (long long)(int)(v & 0xFFFFFFFF);
        default:         return v;
    }
}

long long prog_value(void) {
    long long v = 0;
    switch (prog_base) {
        case BASE_HEX: v = (long long)wcstoll(display_str, NULL, 16); break;
        case BASE_OCT: v = (long long)wcstoll(display_str, NULL,  8); break;
        case BASE_BIN: v = (long long)wcstoll(display_str, NULL,  2); break;
        default:       v = (long long)_wtof(display_str);             break;
    }
    return apply_word_mask(v);
}

void prog_set_display(long long v) {
    v = apply_word_mask(v);
    set_display_int(v);
}

/* ── Error ───────────────────────────────────────────────────────── */

void calc_error(HWND hwnd, const WCHAR *msg) {
    error = TRUE;
    lstrcpynW(display_str, msg, MAX_DISPLAY);
    display_str[MAX_DISPLAY - 1] = L'\0';
    update_ui_text(hwnd);
}