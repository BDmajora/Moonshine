#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <stdlib.h>
#include <wchar.h>
#include "calc.h"
#include "calc_logic.h"
#include "calc_ui.h"
#include "calc_panel.h"

/* ── Menu creation ──────────────────────────────────────────────── */
static HMENU create_menu(void) {
    HMENU hBar  = CreateMenu();
    HMENU hView = CreatePopupMenu();
    HMENU hEdit = CreatePopupMenu();
    HMENU hHelp = CreatePopupMenu();
    HMENU hWork = CreatePopupMenu();

    AppendMenuW(hView, MF_STRING, ID_VIEW_STANDARD,   L"Standard\tAlt+1");
    AppendMenuW(hView, MF_STRING, ID_VIEW_SCIENTIFIC,  L"Scientific\tAlt+2");
    AppendMenuW(hView, MF_STRING, ID_VIEW_PROGRAMMER,  L"Programmer\tAlt+3");
    AppendMenuW(hView, MF_STRING, ID_VIEW_STATISTICS,  L"Statistics\tAlt+4");
    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING, ID_VIEW_HISTORY,     L"History\tCtrl+H");
    AppendMenuW(hView, MF_STRING, ID_VIEW_DIGIT_GRP,   L"Digit grouping");
    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING, ID_VIEW_BASIC,       L"Basic\tCtrl+F4");
    AppendMenuW(hView, MF_STRING, ID_PANEL_UNIT,       L"Unit conversion\tCtrl+U");
    AppendMenuW(hView, MF_STRING, ID_PANEL_DATE,       L"Date calculation\tCtrl+E");

    AppendMenuW(hWork, MF_STRING, ID_WS_MORTGAGE,  L"Mortgage");
    AppendMenuW(hWork, MF_STRING, ID_WS_VEHICLE,   L"Vehicle lease");
    AppendMenuW(hWork, MF_STRING, ID_WS_FUEL_MPG,  L"Fuel economy (mpg)");
    AppendMenuW(hWork, MF_STRING, ID_WS_FUEL_LKM,  L"Fuel economy (L/100 km)");
    AppendMenuW(hView, MF_POPUP,  (UINT_PTR)hWork, L"Worksheets");

    AppendMenuW(hEdit, MF_STRING, 500, L"Copy\tCtrl+C");
    AppendMenuW(hEdit, MF_STRING, 501, L"Paste\tCtrl+V");

    AppendMenuW(hHelp, MF_STRING, ID_HELP_ABOUT, L"About Calculator");

    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hView, L"View");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hEdit, L"Edit");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hHelp, L"Help");

    return hBar;
}

/* ── Window size helpers ────────────────────────────────────────── */
static void get_window_size(int client_w, int client_h, DWORD style,
                            int *ww, int *wh) {
    RECT rc;
    rc.left   = 0;
    rc.top    = 0;
    rc.right  = client_w;
    rc.bottom = client_h;
    AdjustWindowRect(&rc, style, TRUE);
    *ww = rc.right  - rc.left;
    *wh = rc.bottom - rc.top;
}

static int mode_client_width(void) {
    int base;
    int panel_extra = (g_panel != PANEL_NONE) ? 280 : 0;
    switch (g_mode) {
        case MODE_SCIENTIFIC: base = 560; break;
        case MODE_PROGRAMMER: base = 580; break;
        default:              base = 300; break;
    }
    return base + panel_extra;
}

static int mode_client_height(void) {
    switch (g_mode) {
        case MODE_SCIENTIFIC: return 440;
        case MODE_PROGRAMMER: return 460;
        case MODE_STATISTICS: return 460;
        default:              return 420;
    }
}

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

static void sci_unary(HWND hwnd, int id) {
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

/* ── WM_COMMAND dispatcher ───────────────────────────────────────── */
static void on_command(HWND hwnd, int id) {

    /* ── Mode switching ── */
    if (id >= ID_VIEW_STANDARD && id <= ID_VIEW_STATISTICS) {
        DWORD style = (DWORD)GetWindowLongW(hwnd, GWL_STYLE);
        int new_mode = id - ID_VIEW_STANDARD;
        int cw, ch, ww, wh;
        g_mode = new_mode;
        cw = mode_client_width();
        ch = mode_client_height();
        get_window_size(cw, ch, style, &ww, &wh);
        SetWindowPos(hwnd, NULL, 0, 0, ww, wh, SWP_NOMOVE | SWP_NOZORDER);
        ui_rebuild_mode(hwnd);
        ui_update_layout(hwnd);
        ui_update_menu_check(GetMenu(hwnd));
        return;
    }

    /* ── Panel / worksheet switching ── */
    if (id == ID_VIEW_HISTORY) {
        g_panel = (g_panel == PANEL_HISTORY) ? PANEL_NONE : PANEL_HISTORY;
        ui_show_panel(hwnd, g_panel);
        ui_update_menu_check(GetMenu(hwnd));
        return;
    }
    if (id == ID_PANEL_UNIT) {
        g_panel = (g_panel == PANEL_UNIT) ? PANEL_NONE : PANEL_UNIT;
        ui_show_panel(hwnd, g_panel);
        return;
    }
    if (id == ID_PANEL_DATE) {
        g_panel = (g_panel == PANEL_DATE) ? PANEL_NONE : PANEL_DATE;
        ui_show_panel(hwnd, g_panel);
        return;
    }
    if (id >= ID_WS_MORTGAGE && id <= ID_WS_FUEL_LKM) {
        ui_show_worksheet(hwnd, id - ID_WS_MORTGAGE);
        return;
    }
    if (id == ID_VIEW_DIGIT_GRP) {
        digit_grouping = !digit_grouping;
        ui_update_digit_grouping(hwnd);
        ui_update_menu_check(GetMenu(hwnd));
        return;
    }
    if (id == ID_VIEW_BASIC) {
        g_basic_mode = !g_basic_mode;
        ui_rebuild_mode(hwnd);
        ui_update_layout(hwnd);
        ui_update_menu_check(GetMenu(hwnd));
        return;
    }

    /* ── History panel clear ── */
    if (id == ID_PANEL_CLOSE) {
        history_clear();
        ui_update_history_panel(hwnd);
        return;
    }

    /* ── Panel calculate buttons ── */
    if (id == ID_DATE_CALC) { panel_date_calculate(hwnd); return; }
    if (id == ID_WS_CALC)   { panel_ws_calculate(hwnd, g_worksheet); return; }

    /* ── Unit panel: repopulate on type change ── */
    if (id == ID_UNIT_TYPE) {
        HWND hf, ht;
        int cat, i;
        cat = (int)SendMessageW(GetDlgItem(hwnd,ID_UNIT_TYPE), CB_GETCURSEL, 0, 0);
        hf  = GetDlgItem(hwnd, ID_UNIT_FROM_UNT);
        ht  = GetDlgItem(hwnd, ID_UNIT_TO_UNT);
        SendMessageW(hf, CB_RESETCONTENT, 0, 0);
        SendMessageW(ht, CB_RESETCONTENT, 0, 0);
        if (cat >= 0 && cat < g_unit_cat_count) {
            for (i = 0; i < g_unit_cats[cat].unit_count; i++) {
                SendMessageW(hf, CB_ADDSTRING, 0,
                             (LPARAM)g_unit_cats[cat].units[i].name);
                SendMessageW(ht, CB_ADDSTRING, 0,
                             (LPARAM)g_unit_cats[cat].units[i].name);
            }
            SendMessageW(hf, CB_SETCURSEL, 0, 0);
            SendMessageW(ht, CB_SETCURSEL,
                         1 < g_unit_cats[cat].unit_count ? 1 : 0, 0);
        }
        return;
    }
    if (id == ID_UNIT_FROM_UNT || id == ID_UNIT_TO_UNT) {
        panel_unit_convert(hwnd);
        return;
    }

    /* ── Programmer base radios ── */
    if (id == ID_RADIO_HEX) {
        prog_base = BASE_HEX; prog_set_display(prog_value());
        ui_update_display(hwnd); ui_update_prog_buttons(hwnd); return;
    }
    if (id == ID_RADIO_DEC) {
        prog_base = BASE_DEC; set_display((double)prog_value());
        ui_update_display(hwnd); ui_update_prog_buttons(hwnd); return;
    }
    if (id == ID_RADIO_OCT) {
        prog_base = BASE_OCT; prog_set_display(prog_value());
        ui_update_display(hwnd); ui_update_prog_buttons(hwnd); return;
    }
    if (id == ID_RADIO_BIN) {
        prog_base = BASE_BIN; prog_set_display(prog_value());
        ui_update_display(hwnd); ui_update_prog_buttons(hwnd); return;
    }

    /* ── Programmer word radios ── */
    if (id == ID_RADIO_QWORD) {
        prog_word = WORD_QWORD; prog_set_display(prog_value());
        ui_update_display(hwnd);
        if (g_mode == MODE_PROGRAMMER) ui_update_bit_display(hwnd);
        return;
    }
    if (id == ID_RADIO_DWORD) {
        prog_word = WORD_DWORD; prog_set_display(prog_value());
        ui_update_display(hwnd);
        if (g_mode == MODE_PROGRAMMER) ui_update_bit_display(hwnd);
        return;
    }
    if (id == ID_RADIO_WORD) {
        prog_word = WORD_WORD; prog_set_display(prog_value());
        ui_update_display(hwnd);
        if (g_mode == MODE_PROGRAMMER) ui_update_bit_display(hwnd);
        return;
    }
    if (id == ID_RADIO_BYTE) {
        prog_word = WORD_BYTE; prog_set_display(prog_value());
        ui_update_display(hwnd);
        if (g_mode == MODE_PROGRAMMER) ui_update_bit_display(hwnd);
        return;
    }

    /* ── Scientific angle radios ── */
    if (id == ID_RAD_DEG)  { angle_unit = ANGLE_DEG;  return; }
    if (id == ID_RAD_RAD)  { angle_unit = ANGLE_RAD;  return; }
    if (id == ID_RAD_GRAD) { angle_unit = ANGLE_GRAD; return; }

    /* ── Digit buttons ── */
    if (id >= ID_0 && id <= ID_9) {
        handle_digit(hwnd, (WCHAR)(L'0' + (id - ID_0)));
        if (g_mode == MODE_PROGRAMMER) ui_update_bit_display(hwnd);
        return;
    }
    if (id == ID_A) { handle_digit(hwnd, L'A'); return; }
    if (id == ID_B) { handle_digit(hwnd, L'B'); return; }
    if (id == ID_C_HEX) { handle_digit(hwnd, L'C'); return; }
    if (id == ID_D) { handle_digit(hwnd, L'D'); return; }
    if (id == ID_E_HEX) { handle_digit(hwnd, L'E'); return; }
    if (id == ID_F) { handle_digit(hwnd, L'F'); return; }

    /* ── Basic ops ── */
    if (id == ID_DOT) { handle_dot(hwnd); return; }
    if (id == ID_ADD) { handle_op(hwnd, OP_ADD); return; }
    if (id == ID_SUB) { handle_op(hwnd, OP_SUB); return; }
    if (id == ID_MUL) { handle_op(hwnd, OP_MUL); return; }
    if (id == ID_DIV) { handle_op(hwnd, OP_DIV); return; }
    if (id == ID_EQ) {
        do_equals(hwnd);
        if (g_panel == PANEL_HISTORY) ui_update_history_panel(hwnd);
        return;
    }
    if (id == ID_BACK) { handle_back(hwnd); return; }
    if (id == ID_SIGN) {
        if (!error) { set_display(-_wtof(display_str)); ui_update_display(hwnd); }
        return;
    }
    if (id == ID_SQRT) {
        double v;
        if (error) return;
        v = _wtof(display_str);
        if (v < 0) { calc_error(hwnd, L"Invalid input"); return; }
        set_display(sqrt(v)); ui_update_display(hwnd); new_input = TRUE;
        return;
    }
    if (id == ID_PERCENT) {
        if (!error) { set_display(_wtof(display_str) / 100.0); ui_update_display(hwnd); }
        return;
    }
    if (id == ID_RECIP) {
        double v;
        if (error) return;
        v = _wtof(display_str);
        if (v == 0) { calc_error(hwnd, L"Cannot divide by zero"); return; }
        set_display(1.0 / v); ui_update_display(hwnd); new_input = TRUE;
        return;
    }

    /* ── Clear ── */
    if (id == ID_CLR) {
        current = operand = 0.0; op = OP_NONE;
        has_operand = FALSE; new_input = TRUE; error = FALSE;
        wcscpy(display_str, L"0"); ui_update_display(hwnd);
        if (g_mode == MODE_STATISTICS) { stat_clear_all(); ui_update_stat_display(hwnd); }
        return;
    }
    if (id == ID_CE) {
        error = FALSE; wcscpy(display_str, L"0"); new_input = TRUE;
        ui_update_display(hwnd); return;
    }

    /* ── Memory ── */
    if (id == ID_MC)     { memory = 0.0; return; }
    if (id == ID_MR)     { if (!error) { set_display(memory); ui_update_display(hwnd); new_input=TRUE; } return; }
    if (id == ID_MS)     { if (!error) { memory = _wtof(display_str); new_input=TRUE; } return; }
    if (id == ID_MPLUS)  { if (!error) memory += _wtof(display_str); return; }
    if (id == ID_MMINUS) { if (!error) memory -= _wtof(display_str); return; }

    /* ── Scientific functions ── */
    if (id == ID_SCI_INV) { inv_mode = !inv_mode; return; }
    if (id == ID_SCI_FE) {
        fe_mode = !fe_mode;
        if (!error) { set_display(_wtof(display_str)); ui_update_display(hwnd); }
        return;
    }
    if (id == ID_SCI_PI)     { set_display(3.14159265358979323846); ui_update_display(hwnd); new_input=TRUE; return; }
    if (id == ID_SCI_MOD)    { handle_op(hwnd, OP_MOD); return; }
    if (id == ID_SCI_POWY)   { handle_op(hwnd, OP_POW); return; }
    if (id == ID_SCI_YROOTX) { handle_op(hwnd, OP_NROOT); return; }
    if (id == ID_SCI_LPAREN) { handle_paren_open(hwnd); return; }
    if (id == ID_SCI_RPAREN) { handle_paren_close(hwnd); return; }

    if (id == ID_SCI_SIN   || id == ID_SCI_COS   || id == ID_SCI_TAN  ||
        id == ID_SCI_SINH  || id == ID_SCI_COSH  || id == ID_SCI_TANH ||
        id == ID_SCI_LOG   || id == ID_SCI_LN    || id == ID_SCI_POW2 ||
        id == ID_SCI_POW3  || id == ID_SCI_CUBE  || id == ID_SCI_FACT ||
        id == ID_SCI_INT   || id == ID_SCI_DMS) {
        sci_unary(hwnd, id);
        return;
    }

    /* ── Programmer bitwise ── */
    if (id == ID_PROG_AND) { handle_op(hwnd, OP_AND); return; }
    if (id == ID_PROG_OR)  { handle_op(hwnd, OP_OR);  return; }
    if (id == ID_PROG_XOR) { handle_op(hwnd, OP_XOR); return; }
    if (id == ID_PROG_MOD) { handle_op(hwnd, OP_MOD); return; }
    if (id == ID_PROG_LSH) { handle_op(hwnd, OP_LSH); return; }
    if (id == ID_PROG_RSH) { handle_op(hwnd, OP_RSH); return; }
    if (id == ID_PROG_NOT) {
        long long v = prog_value();
        prog_set_display(~v);
        ui_update_display(hwnd); ui_update_bit_display(hwnd);
        new_input = TRUE;
        return;
    }
    if (id == ID_PROG_ROL) {
        unsigned long long v;
        unsigned long long mask;
        int bits;
        v    = (unsigned long long)prog_value();
        bits = (prog_word == WORD_BYTE  ?  8 :
                prog_word == WORD_WORD  ? 16 :
                prog_word == WORD_DWORD ? 32 : 64);
        mask = (bits == 64) ? ~0ULL : (1ULL << bits) - 1;
        v   &= mask;
        v    = ((v << 1) | (v >> (bits - 1))) & mask;
        prog_set_display((long long)v);
        ui_update_display(hwnd); ui_update_bit_display(hwnd); new_input = TRUE;
        return;
    }
    if (id == ID_PROG_ROR) {
        unsigned long long v;
        unsigned long long mask;
        int bits;
        v    = (unsigned long long)prog_value();
        bits = (prog_word == WORD_BYTE  ?  8 :
                prog_word == WORD_WORD  ? 16 :
                prog_word == WORD_DWORD ? 32 : 64);
        mask = (bits == 64) ? ~0ULL : (1ULL << bits) - 1;
        v   &= mask;
        v    = ((v >> 1) | (v << (bits - 1))) & mask;
        prog_set_display((long long)v);
        ui_update_display(hwnd); ui_update_bit_display(hwnd); new_input = TRUE;
        return;
    }

    /* ── Statistics ── */
    if (id == ID_STAT_ADD) {
        if (!error) {
            stat_add(_wtof(display_str));
            ui_update_stat_display(hwnd);
            wcscpy(display_str, L"0"); new_input = TRUE;
        }
        return;
    }
    if (id == ID_STAT_CAD) {
        HWND hl = GetDlgItem(hwnd, ID_DISP_HISTORY);
        int sel = (int)SendMessageW(hl, LB_GETCURSEL, 0, 0);
        if (sel >= 0 && sel < stat_count) {
            int i;
            for (i = sel; i < stat_count - 1; i++)
                stat_data[i] = stat_data[i + 1];
            stat_count--;
            ui_update_stat_display(hwnd);
        }
        return;
    }
    if (id == ID_STAT_MEAN)  { set_display(stat_mean());      ui_update_display(hwnd); new_input=TRUE; return; }
    if (id == ID_STAT_MEAN2) { set_display(stat_mean_sq());   ui_update_display(hwnd); new_input=TRUE; return; }
    if (id == ID_STAT_SUMX)  { set_display(stat_sum());       ui_update_display(hwnd); new_input=TRUE; return; }
    if (id == ID_STAT_SUMX2) { set_display(stat_sum_sq());    ui_update_display(hwnd); new_input=TRUE; return; }
    if (id == ID_STAT_SDEV)  { set_display(stat_sdev(TRUE));  ui_update_display(hwnd); new_input=TRUE; return; }
    if (id == ID_STAT_SDEV1) { set_display(stat_sdev(FALSE)); ui_update_display(hwnd); new_input=TRUE; return; }

    /* ── Edit menu ── */
    if (id == 500) { /* Copy */
        if (OpenClipboard(hwnd)) {
            SIZE_T sz = (wcslen(display_str) + 1) * sizeof(WCHAR);
            HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, sz);
            if (hg) {
                WCHAR *p = (WCHAR *)GlobalLock(hg);
                if (p) {
                    wcscpy(p, display_str);
                    GlobalUnlock(hg);
                    EmptyClipboard();
                    SetClipboardData(CF_UNICODETEXT, hg);
                }
            }
            CloseClipboard();
        }
        return;
    }
    if (id == 501) { /* Paste */
        if (OpenClipboard(hwnd)) {
            HANDLE h = GetClipboardData(CF_UNICODETEXT);
            if (h) {
                WCHAR *p = (WCHAR *)GlobalLock(h);
                if (p) {
                    lstrcpynW(display_str, p, MAX_DISPLAY - 1);
                    display_str[MAX_DISPLAY - 1] = L'\0';
                    ui_update_display(hwnd);
                    new_input = TRUE;
                }
                GlobalUnlock(h);
            }
            CloseClipboard();
        }
        return;
    }

    /* ── About ── */
    if (id == ID_HELP_ABOUT) {
        MessageBoxW(hwnd,
            L"Calculator\n\nWindows 7 Calculator Clone\nBuilt in C/Win32",
            L"About Calculator", MB_OK | MB_ICONINFORMATION);
        return;
    }
}

/* ── Keyboard handling ───────────────────────────────────────────── */
static void on_keydown(HWND hwnd, WPARAM vk, BOOL ctrl, BOOL alt) {
    if (alt) {
        if (vk == '1') { on_command(hwnd, ID_VIEW_STANDARD);   return; }
        if (vk == '2') { on_command(hwnd, ID_VIEW_SCIENTIFIC);  return; }
        if (vk == '3') { on_command(hwnd, ID_VIEW_PROGRAMMER);  return; }
        if (vk == '4') { on_command(hwnd, ID_VIEW_STATISTICS);  return; }
        return;
    }
    if (ctrl) {
        if (vk == 'C') { on_command(hwnd, 500); return; }
        if (vk == 'V') { on_command(hwnd, 501); return; }
        if (vk == 'H') { on_command(hwnd, ID_VIEW_HISTORY); return; }
        if (vk == 'U') { on_command(hwnd, ID_PANEL_UNIT);   return; }
        if (vk == 'E') { on_command(hwnd, ID_PANEL_DATE);   return; }
        return;
    }
    switch (vk) {
        case VK_ESCAPE: on_command(hwnd, ID_CLR);  break;
        case VK_DELETE: on_command(hwnd, ID_CE);   break;
        case VK_BACK:   on_command(hwnd, ID_BACK); break;
        case VK_RETURN: on_command(hwnd, ID_EQ);   break;
        case VK_F9:     on_command(hwnd, ID_SIGN); break;
        default: break;
    }
}

static void on_char(HWND hwnd, WCHAR ch) {
    if (ch >= L'0' && ch <= L'9') { on_command(hwnd, ID_0 + (ch - L'0')); return; }
    if (ch >= L'a' && ch <= L'f') ch = (WCHAR)(ch - L'a' + L'A');
    switch (ch) {
        case L'A': on_command(hwnd, ID_A);       break;
        case L'B': on_command(hwnd, ID_B);       break;
        case L'C': on_command(hwnd, ID_C_HEX);  break;
        case L'D': on_command(hwnd, ID_D);       break;
        case L'E': on_command(hwnd, ID_E_HEX);  break;
        case L'F': on_command(hwnd, ID_F);       break;
        case L'+': on_command(hwnd, ID_ADD);     break;
        case L'-': on_command(hwnd, ID_SUB);     break;
        case L'*': on_command(hwnd, ID_MUL);     break;
        case L'/': on_command(hwnd, ID_DIV);     break;
        case L'.': on_command(hwnd, ID_DOT);     break;
        case L'=': on_command(hwnd, ID_EQ);      break;
        case L'%': on_command(hwnd, ID_PERCENT); break;
        case L'@': on_command(hwnd, ID_SQRT);    break;
        case 27:   on_command(hwnd, ID_CLR);     break;
        case 8:    on_command(hwnd, ID_BACK);    break;
        case 13:   on_command(hwnd, ID_EQ);      break;
        default:   break;
    }
}

/* ── Window Procedure ────────────────────────────────────────────── */
static LRESULT CALLBACK CalcWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            ui_create_controls(hwnd);
            break;

        case WM_SIZE:
            ui_update_layout(hwnd);
            break;

        case WM_COMMAND: {
            int id    = (int)LOWORD(wp);
            int notif = (int)HIWORD(wp);
            if (notif == CBN_SELCHANGE || notif == 0 || notif == BN_CLICKED)
                on_command(hwnd, id);
            if (id == ID_UNIT_FROM_VAL && notif == EN_CHANGE)
                panel_unit_convert(hwnd);
            break;
        }

        case WM_KEYDOWN: {
            BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            BOOL alt  = (GetKeyState(VK_MENU)    & 0x8000) != 0;
            on_keydown(hwnd, wp, ctrl, alt);
            break;
        }

        case WM_CHAR:
            on_char(hwnd, (WCHAR)wp);
            break;

        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO *)lp;
            DWORD style = (DWORD)GetWindowLongW(hwnd, GWL_STYLE);
            int mw = (g_mode == MODE_STANDARD ? 280 : 420);
            int mh = 380;
            int ww, wh;
            get_window_size(mw, mh, style, &ww, &wh);
            mmi->ptMinTrackSize.x = ww;
            mmi->ptMinTrackSize.y = wh;
            return 0;
        }

        case WM_DESTROY:
            if (hBtnFont)   DeleteObject(hBtnFont);
            if (hDispFont)  DeleteObject(hDispFont);
            if (hSmallFont) DeleteObject(hSmallFont);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

/* ── Entry point ─────────────────────────────────────────────────── */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev,
                    LPWSTR cmdline, int cmdshow) {
    WNDCLASSW  wc;
    HWND       hwnd;
    MSG        msg;
    HMENU      hMenu;
    DWORD      dwStyle = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX;
    int        cw, ch, ww, wh;
    INITCOMMONCONTROLSEX icc;

    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = CalcWndProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"CalcWnd";
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hIcon         = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    RegisterClassW(&wc);

    hMenu = create_menu();
    cw = mode_client_width();
    ch = mode_client_height();
    get_window_size(cw, ch, dwStyle, &ww, &wh);

    hwnd = CreateWindowW(L"CalcWnd", L"Calculator",
                         dwStyle,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         ww, wh,
                         NULL, hMenu, hInstance, NULL);

    ui_update_menu_check(hMenu);
    ShowWindow(hwnd, cmdshow);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    (void)hPrev;
    (void)cmdline;
    return (int)msg.wParam;
}