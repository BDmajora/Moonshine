#include <windows.h>
#include "calc_ui.h"

/* 
 * NOTE: Global state (g_mode, g_panel, etc.) and layout logic (compute_layout, 
 * recreate_fonts) have been moved to calc_update.c to avoid linker collisions.
 */

/* ── Control Manipulation ── */

void move_ctrl(HWND hwnd, int id, int x, int y, int w, int h) {
    HWND c = GetDlgItem(hwnd, id);
    if (c) MoveWindow(c, x, y, w, h, TRUE);
}

/* ── Control Factories ── */

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