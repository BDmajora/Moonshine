/* calc_update.c — UI state, layout dispatch, mode/panel switching. */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <math.h>
#include <wchar.h>
#include <stdlib.h>
#include "calc.h"
#include "calc_defs.h"
#include "calc_logic.h"
#include "calc_panel.h"

extern void ui_destroy_children(void);

int   g_mode       = MODE_STANDARD;
int   g_panel      = PANEL_NONE;
int   g_worksheet  = WS_MORTGAGE;
BOOL  g_basic_mode = FALSE;

HFONT hBtnFont   = NULL;
HFONT hDispFont  = NULL;
HFONT hSmallFont = NULL;

static BOOL CALLBACK SetFontProc(HWND h, LPARAM lp) {
    SendMessageW(h, WM_SETFONT, (WPARAM)lp, TRUE);
    return TRUE;
}

void recreate_fonts(int btn_h, int disp_h) {
    if (hBtnFont)   DeleteObject(hBtnFont);
    if (hDispFont)  DeleteObject(hDispFont);
    if (hSmallFont) DeleteObject(hSmallFont);
    hBtnFont   = CreateFontW((int)(btn_h * 0.38), 0, 0, 0, FW_NORMAL,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    hDispFont  = CreateFontW((int)(disp_h * 0.55), 0, 0, 0, FW_LIGHT,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    hSmallFont = CreateFontW((int)(btn_h * 0.28), 0, 0, 0, FW_NORMAL,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

void compute_layout(HWND hwnd, Layout *L) {
    RECT rc;
    int header_h = 0;
    GetClientRect(hwnd, &rc);
    L->win_w = rc.right;
    L->win_h = rc.bottom;
    L->margin = 12;
    L->gap    = 6;
    L->disp_h = 60;
    if (g_panel != PANEL_NONE) {
        L->panel_w = 380;  /* must match GM_PANEL_W in calc_window.c */
        L->panel_x = L->win_w - L->panel_w;
        L->grid_w  = L->panel_x - (L->margin * 2);
    } else {
        L->panel_w = 0;
        L->panel_x = L->win_w;
        L->grid_w  = L->win_w - (L->margin * 2);
    }
    L->disp_x = L->margin;
    L->disp_y = L->margin;
    L->disp_w = L->grid_w;
    switch (g_mode) {
        case MODE_SCIENTIFIC: L->cols = 10; L->rows = 7; header_h = 30;  break;
        case MODE_PROGRAMMER: L->cols = 9;  L->rows = 7; header_h = 50;  break;
        case MODE_STATISTICS: L->cols = 5;  L->rows = 7; header_h = 100; break;
        default:              L->cols = 5;  L->rows = 6; header_h = 0;   break;
    }
    L->grid_y = L->disp_y + L->disp_h + L->margin + header_h;
    L->grid_h = L->win_h - L->grid_y - L->margin;
    L->bw = (L->grid_w - (L->gap * (L->cols - 1))) / L->cols;
    L->bh = (L->grid_h - (L->gap * (L->rows - 1))) / L->rows;
}

void ui_update_layout(HWND hwnd) {
    Layout L;
    compute_layout(hwnd, &L);
    if (L.bw <= 0 || L.bh <= 0 || L.win_w <= 0 || L.win_h <= 0)
        return;
    recreate_fonts(L.bh, L.disp_h);
    move_ctrl(hwnd, ID_DISPLAY, L.disp_x, L.disp_y, L.disp_w, L.disp_h);
    switch (g_mode) {
        case MODE_SCIENTIFIC: layout_scientific(hwnd, &L); break;
        case MODE_PROGRAMMER: layout_programmer(hwnd, &L); break;
        case MODE_STATISTICS: layout_statistics(hwnd, &L); break;
        default:              layout_standard(hwnd, &L);   break;
    }
    if (g_panel != PANEL_NONE)
        layout_panel(hwnd, &L);
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
    int bits, i, pos;
    if (!hb) return;
    v = prog_value();
    uv = (unsigned long long)v;
    bits = 64;
    switch (prog_word) {
        case WORD_BYTE:  bits = 8;  uv &= 0xFFULL;        break;
        case WORD_WORD:  bits = 16; uv &= 0xFFFFULL;      break;
        case WORD_DWORD: bits = 32; uv &= 0xFFFFFFFFULL;  break;
        default: break;
    }
    pos = 0;
    for (i = bits - 1; i >= 0; i--) {
        buf[pos++] = ((uv >> i) & 1) ? L'1' : L'0';
        if (i % 4 == 0 && i > 0) buf[pos++] = L' ';
    }
    buf[pos] = L'\0';
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

void ui_update_digit_grouping(HWND hwnd) {
    WCHAR tmp[MAX_DISPLAY], out[MAX_DISPLAY * 2];
    double v;
    long long iv;
    BOOL neg;
    int len, i, j;
    if (!digit_grouping) { ui_update_display(hwnd); return; }
    v = _wtof(display_str);
    if (error || !(v == (long long)v && fabs(v) < 1e15)) return;
    iv = (long long)v;
    neg = (iv < 0);
    if (neg) iv = -iv;
    swprintf(tmp, MAX_DISPLAY, L"%lld", iv);
    len = (int)wcslen(tmp);
    j = 0;
    for (i = 0; i < len; i++) {
        if (i > 0 && (len - i) % 3 == 0) out[j++] = L',';
        out[j++] = tmp[i];
    }
    out[j] = L'\0';
    if (neg) swprintf(display_str, MAX_DISPLAY, L"-%s", out);
    else     lstrcpynW(display_str, out, MAX_DISPLAY - 1);
    ui_update_display(hwnd);
}

void ui_update_history_panel(HWND hwnd) {
    int i;
    HWND hl = GetDlgItem(hwnd, ID_PANEL_COMBO);
    if (!hl) return;
    SendMessageW(hl, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < hist_count; i++)
        SendMessageW(hl, LB_ADDSTRING, 0, (LPARAM)history[i]);
    if (hist_count > 0)
        SendMessageW(hl, LB_SETCURSEL, hist_count - 1, 0);
}

void ui_update_prog_buttons(HWND hwnd) {
    int id;
    BOOL hex = (prog_base == BASE_HEX);
    BOOL dec = (prog_base == BASE_DEC || hex);
    BOOL oct = (prog_base == BASE_OCT || dec);
    for (id = ID_A; id <= ID_F; id++)
        EnableWindow(GetDlgItem(hwnd, id), hex);
    EnableWindow(GetDlgItem(hwnd, ID_8), dec);
    EnableWindow(GetDlgItem(hwnd, ID_9), dec);
    for (id = ID_2; id <= ID_7; id++)
        EnableWindow(GetDlgItem(hwnd, id), oct);
}

void ui_rebuild_mode(HWND hwnd) {
    ui_destroy_children();
    ui_create_controls(hwnd);
    ui_update_display(hwnd);
}

void ui_switch_mode(HWND hwnd, int new_mode) {
    g_mode = new_mode;
    ui_rebuild_mode(hwnd);
    apply_window_size(hwnd, g_mode, g_panel);
    ui_update_menu_check(GetMenu(hwnd));
}

void ui_show_panel(HWND hwnd, int new_panel) {
    g_panel = (g_panel == new_panel) ? PANEL_NONE : new_panel;
    ui_rebuild_mode(hwnd);
    apply_window_size(hwnd, g_mode, g_panel);
    ui_update_menu_check(GetMenu(hwnd));
}

void ui_show_worksheet(HWND hwnd, int ws_type) {
    if (g_panel == PANEL_WORKSHEET && g_worksheet == ws_type)
        g_panel = PANEL_NONE;
    else { g_worksheet = ws_type; g_panel = PANEL_WORKSHEET; }
    ui_rebuild_mode(hwnd);
    apply_window_size(hwnd, g_mode, g_panel);
    ui_update_menu_check(GetMenu(hwnd));
}

void ui_update_menu_check(HMENU hMenu) {
    int i;
    static const int mode_ids[4] = {
        ID_VIEW_STANDARD, ID_VIEW_SCIENTIFIC,
        ID_VIEW_PROGRAMMER, ID_VIEW_STATISTICS
    };
    static const int ws_ids[4] = {
        ID_WS_MORTGAGE, ID_WS_VEHICLE,
        ID_WS_FUEL_MPG, ID_WS_FUEL_LKM
    };

    /* Mode bullets */
    for (i = 0; i < 4; i++)
        CheckMenuItem(hMenu, mode_ids[i], MF_BYCOMMAND |
            (g_mode == i ? MF_CHECKED : MF_UNCHECKED));

    /* Panel toggles */
    CheckMenuItem(hMenu, ID_VIEW_HISTORY,
        g_panel == PANEL_HISTORY ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_PANEL_UNIT,
        g_panel == PANEL_UNIT    ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_PANEL_DATE,
        g_panel == PANEL_DATE    ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_VIEW_DIGIT_GRP,
        digit_grouping           ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, ID_VIEW_BASIC,
        g_basic_mode             ? MF_CHECKED : MF_UNCHECKED);

    /* Worksheet sub-menu bullets */
    for (i = 0; i < 4; i++)
        CheckMenuItem(hMenu, ws_ids[i], MF_BYCOMMAND |
            ((g_panel == PANEL_WORKSHEET && g_worksheet == i)
             ? MF_CHECKED : MF_UNCHECKED));
}