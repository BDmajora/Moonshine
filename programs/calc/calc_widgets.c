#include "calc_ui.h"
#include <math.h>

/* Global UI state definitions */
int   g_mode       = MODE_STANDARD;
int   g_panel      = PANEL_NONE;
int   g_worksheet  = WS_MORTGAGE;
BOOL  g_basic_mode = FALSE;

HFONT hBtnFont   = NULL;
HFONT hDispFont  = NULL;
HFONT hSmallFont = NULL;

void move_ctrl(HWND hwnd, int id, int x, int y, int w, int h) {
    HWND c = GetDlgItem(hwnd, id);
    if (c) MoveWindow(c, x, y, w, h, TRUE);
}

HWND make_button(HWND parent, int id, const WCHAR *label, BOOL def_btn) {
    DWORD style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
    if (def_btn) style |= BS_DEFPUSHBUTTON;
    return CreateWindowW(L"BUTTON", label, style,
                         0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

HWND make_radio(HWND parent, int id, const WCHAR *label, BOOL first_in_group) {
    DWORD style = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
    if (first_in_group) style |= WS_GROUP;
    return CreateWindowW(L"BUTTON", label, style,
                         0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

HWND make_label(HWND parent, int id, const WCHAR *text) {
    return CreateWindowW(L"STATIC", text,
                         WS_CHILD | WS_VISIBLE | SS_LEFT,
                         0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

HWND make_edit(HWND parent, int id, const WCHAR *placeholder) {
    return CreateWindowW(L"EDIT", placeholder,
                         WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                         0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

HWND make_combo(HWND parent, int id) {
    return CreateWindowW(L"COMBOBOX", NULL,
                         WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                         CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                         0, 0, 0, 200, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

void recreate_fonts(int btn_h, int disp_h) {
    if (hBtnFont)   DeleteObject(hBtnFont);
    if (hDispFont)  DeleteObject(hDispFont);
    if (hSmallFont) DeleteObject(hSmallFont);

    hBtnFont   = CreateFontW((int)(btn_h * 0.38), 0, 0, 0, FW_NORMAL,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                              L"Segoe UI");
    hDispFont  = CreateFontW((int)(disp_h * 0.55), 0, 0, 0, FW_LIGHT,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                              L"Segoe UI");
    hSmallFont = CreateFontW((int)(btn_h * 0.28), 0, 0, 0, FW_NORMAL,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                              L"Segoe UI");
}

void compute_layout(HWND hwnd, Layout *L) {
    RECT rc;
    int header_h;

    GetClientRect(hwnd, &rc);
    L->win_w = rc.right;
    L->win_h = rc.bottom;

    L->margin = max(6,  (int)(L->win_w * 0.018));
    L->gap    = max(3,  (int)(L->win_w * 0.008));
    L->disp_h = max(50, (int)(L->win_h * 0.14));

    if (g_panel != PANEL_NONE) {
        L->panel_w = max(260, (int)(L->win_w * 0.45));
        L->panel_x = L->win_w - L->panel_w;
        L->grid_w  = L->panel_x - L->margin * 2;
    } else {
        L->panel_w = 0;
        L->panel_x = L->win_w;
        L->grid_w  = L->win_w - L->margin * 2;
    }

    L->disp_x = L->margin;
    L->disp_y = L->margin;
    L->disp_w = L->grid_w;

    switch (g_mode) {
        case MODE_SCIENTIFIC: L->cols = 10; L->rows = 7; break;
        case MODE_PROGRAMMER: L->cols = 9;  L->rows = 7; break;
        case MODE_STATISTICS: L->cols = 5;  L->rows = 7; break;
        default:              L->cols = 5;  L->rows = 6; break;
    }

    header_h = 0;
    if (g_mode == MODE_SCIENTIFIC) header_h = (int)(L->win_h * 0.05);
    if (g_mode == MODE_PROGRAMMER) header_h = (int)(L->win_h * 0.10);
    if (g_mode == MODE_STATISTICS) header_h = (int)(L->win_h * 0.25);

    L->grid_y = L->disp_y + L->disp_h + L->margin + header_h;
    L->grid_h = L->win_h - L->grid_y - L->margin;

    L->bw = (L->grid_w - L->gap * (L->cols - 1)) / L->cols;
    L->bh = (L->grid_h - L->gap * (L->rows - 1)) / L->rows;
    L->bh = max(L->bh, 24);
    L->bw = max(L->bw, 20);
}