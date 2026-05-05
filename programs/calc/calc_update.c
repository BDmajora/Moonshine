#include "calc_ui.h"
#include "calc_logic.h"
#include <stdio.h>
#include <math.h>

/* function prototype to fix implicit declaration warning */
double _wtof(const WCHAR *);

static BOOL CALLBACK SetFontProc(HWND h, LPARAM lp) {
    SendMessageW(h, WM_SETFONT, (WPARAM)lp, TRUE);
    return TRUE;
}

static BOOL CALLBACK DestroyChildProc(HWND h, LPARAM lp) {
    DestroyWindow(h);
    (void)lp;
    return TRUE;
}

void ui_update_layout(HWND hwnd) {
    Layout L;
    compute_layout(hwnd, &L);
    recreate_fonts(L.bh, L.disp_h);

    move_ctrl(hwnd, ID_DISPLAY, L.disp_x, L.disp_y, L.disp_w, L.disp_h);

    switch (g_mode) {
        case MODE_SCIENTIFIC: layout_scientific(hwnd, &L); break;
        case MODE_PROGRAMMER: layout_programmer(hwnd, &L); break;
        case MODE_STATISTICS: layout_statistics(hwnd, &L); break;
        default:              layout_standard(hwnd, &L);   break;
    }

    if (g_panel != PANEL_NONE) layout_panel(hwnd, &L);

    EnumChildWindows(hwnd, SetFontProc, (LPARAM)hBtnFont);
    SendMessageW(GetDlgItem(hwnd, ID_DISPLAY), WM_SETFONT, (WPARAM)hDispFont, TRUE);

    InvalidateRect(hwnd, NULL, TRUE);
}

void ui_update_display(HWND hwnd) {
    HWND hd = GetDlgItem(hwnd, ID_DISPLAY);
    if (hd) SetWindowTextW(hd, display_str);
}

void ui_update_bit_display(HWND hwnd) {
    HWND hb = GetDlgItem(hwnd, ID_DISP_BITS);
    WCHAR buf[256];
    unsigned long long uv;
    long long v;
    int bits, i, pos, buflen;

    if (!hb) return;
    buf[0] = L'\0';
    v   = prog_value();
    uv  = (unsigned long long)v;
    bits = 64;
    switch (prog_word) {
        case WORD_BYTE:  bits = 8;  uv &= 0xFFULL;        break;
        case WORD_WORD:  bits = 16; uv &= 0xFFFFULL;      break;
        case WORD_DWORD: bits = 32; uv &= 0xFFFFFFFFULL;  break;
        default:                                           break;
    }
    pos = 0;
    for (i = bits - 1; i >= 0; i--) {
        buf[pos++] = ((uv >> i) & 1) ? L'1' : L'0';
        if (i % 4 == 0 && i > 0) buf[pos++] = L' ';
    }
    buf[pos] = L'\0';
    buflen = pos; (void)buflen;
    SetWindowTextW(hb, buf);
}

void ui_update_stat_display(HWND hwnd) {
    HWND hl = GetDlgItem(hwnd, ID_DISP_HISTORY);
    WCHAR lbl[64];
    int i;
    if (!hl) return;
    SendMessageW(hl, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < stat_count; i++) {
        swprintf(lbl, 64, L"%.10g", stat_data[i]);
        SendMessageW(hl, LB_ADDSTRING, 0, (LPARAM)lbl);
    }
    swprintf(lbl, 64, L"Count = %d", stat_count);
    SetWindowTextW(GetDlgItem(hwnd, ID_DISPLAY), lbl);
}

void ui_update_history_panel(HWND hwnd) {
    HWND hl = GetDlgItem(hwnd, ID_PANEL_COMBO);
    int i;
    if (!hl) return;
    SendMessageW(hl, LB_RESETCONTENT, 0, 0);
    for (i = hist_count - 1; i >= 0; i--)
        SendMessageW(hl, LB_ADDSTRING, 0, (LPARAM)history[i]);
}

void ui_update_prog_buttons(HWND hwnd) {
    static const int all_hex[] = {ID_A,ID_B,ID_C_HEX,ID_D,ID_E_HEX,ID_F,0};
    static const int dec_off[] = {ID_A,ID_B,ID_C_HEX,ID_D,ID_E_HEX,ID_F,0};
    static const int oct_off[] = {ID_8,ID_9,ID_A,ID_B,ID_C_HEX,ID_D,ID_E_HEX,ID_F,0};
    static const int bin_off[] = {ID_2,ID_3,ID_4,ID_5,ID_6,ID_7,ID_8,ID_9,
                                   ID_A,ID_B,ID_C_HEX,ID_D,ID_E_HEX,ID_F,0};
    const int *off;
    int i;

    for (i = 0; all_hex[i]; i++)
        EnableWindow(GetDlgItem(hwnd, all_hex[i]), prog_base == BASE_HEX);

    switch (prog_base) {
        case BASE_HEX: return;
        case BASE_DEC: off = dec_off; break;
        case BASE_OCT: off = oct_off; break;
        default:       off = bin_off; break;
    }
    for (i = 0; off[i]; i++)
        EnableWindow(GetDlgItem(hwnd, off[i]), FALSE);
}

void ui_update_digit_grouping(HWND hwnd) {
    WCHAR tmp[MAX_DISPLAY];
    WCHAR out[MAX_DISPLAY * 2];
    double v;
    long long iv;
    BOOL neg;
    int len, i2, j;

    if (!digit_grouping) { ui_update_display(hwnd); return; }
    v = _wtof(display_str);
    if (error || !(v == (long long)v && fabs(v) < 1e15)) return;

    iv  = (long long)v;
    neg = (iv < 0);
    if (neg) iv = -iv;
    swprintf(tmp, MAX_DISPLAY, L"%lld", iv);
    len = (int)wcslen(tmp);
    j = 0;
    for (i2 = 0; i2 < len; i2++) {
        if (i2 > 0 && (len - i2) % 3 == 0) out[j++] = L',';
        out[j++] = tmp[i2];
    }
    out[j] = L'\0';

    if (neg)
        swprintf(display_str, MAX_DISPLAY, L"-%s", out);
    else
        lstrcpynW(display_str, out, MAX_DISPLAY - 1);

    ui_update_display(hwnd);
}

void ui_show_panel(HWND hwnd, int panel) {
    g_panel = panel;
    ui_rebuild_mode(hwnd);
    ui_update_layout(hwnd);
}

void ui_show_worksheet(HWND hwnd, int ws_type) {
    g_worksheet = ws_type;
    g_panel = PANEL_WORKSHEET;
    ui_rebuild_mode(hwnd);
    ui_update_layout(hwnd);
}

void ui_rebuild_mode(HWND hwnd) {
    EnumChildWindows(hwnd, DestroyChildProc, 0);
    ui_create_controls(hwnd);
    ui_update_display(hwnd);
}

void ui_update_menu_check(HMENU hMenu) {
    int modes[4];
    int i;
    modes[0] = ID_VIEW_STANDARD;
    modes[1] = ID_VIEW_SCIENTIFIC;
    modes[2] = ID_VIEW_PROGRAMMER;
    modes[3] = ID_VIEW_STATISTICS;
    for (i = 0; i < 4; i++)
        CheckMenuItem(hMenu, modes[i], MF_BYCOMMAND |
            (g_mode == i ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_VIEW_HISTORY,
        g_panel == PANEL_HISTORY ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_VIEW_DIGIT_GRP,
        digit_grouping ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_VIEW_BASIC,
        g_basic_mode ? MF_CHECKED : MF_UNCHECKED);
}