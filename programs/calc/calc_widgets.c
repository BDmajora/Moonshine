#include <windows.h>
#include "calc_defs.h"

void move_ctrl(HWND hwnd, int id, int x, int y, int w, int h) {
    HWND c = GetDlgItem(hwnd, id);
    if (c) MoveWindow(c, x, y, w, h, TRUE);
}

/* Every factory wraps CreateWindowW and calls ui_register_child so
   ui_rebuild_mode always has a complete list to destroy. */

HWND make_button(HWND parent, int id, const WCHAR *label, BOOL def_btn) {
    DWORD style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
    HWND h;
    if (def_btn) style |= BS_DEFPUSHBUTTON;
    h = CreateWindowW(L"BUTTON", label, style,
                      0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    ui_register_child(h);
    return h;
}

HWND make_radio(HWND parent, int id, const WCHAR *label, BOOL first_in_group) {
    DWORD style = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
    HWND h;
    if (first_in_group) style |= WS_GROUP;
    h = CreateWindowW(L"BUTTON", label, style,
                      0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    ui_register_child(h);
    return h;
}

HWND make_label(HWND parent, int id, const WCHAR *text) {
    HWND h = CreateWindowW(L"STATIC", text,
                           WS_CHILD | WS_VISIBLE | SS_LEFT,
                           0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    ui_register_child(h);
    return h;
}

HWND make_edit(HWND parent, int id, const WCHAR *placeholder) {
    HWND h = CreateWindowW(L"EDIT", placeholder,
                           WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                           0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    ui_register_child(h);
    return h;
}

HWND make_combo(HWND parent, int id) {
    HWND h = CreateWindowW(L"COMBOBOX", NULL,
                           WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                           CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                           0, 0, 0, 200, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    ui_register_child(h);
    return h;
}