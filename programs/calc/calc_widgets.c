/* ────────────────────────────────────────────────────────────────────
   calc_widgets.c — control factories and the child-window registry.

   Every HWND we create is registered in s_children[] so that
   ui_rebuild_mode() can destroy exactly the controls it created on
   the previous mode/panel. No EnumChildWindows, no Z-order walks.

   Moved registry here (from calc_update.c) so factories and the list
   they populate live in one file. move_ctrl uses bRepaint=FALSE; the
   layout pass invalidates the whole window once at the end instead.
   That removes the ghost-button flicker seen when switching modes.
   ──────────────────────────────────────────────────────────────────── */
#include <windows.h>
#include "calc_defs.h"

#define MAX_CHILDREN 600
static HWND s_children[MAX_CHILDREN];
static int  s_child_count = 0;

void ui_register_child(HWND h) {
    if (h && s_child_count < MAX_CHILDREN)
        s_children[s_child_count++] = h;
}

void ui_destroy_children(void) {
    int i;
    for (i = 0; i < s_child_count; i++) {
        if (s_children[i] && IsWindow(s_children[i]))
            DestroyWindow(s_children[i]);
        s_children[i] = NULL;
    }
    s_child_count = 0;
}

void move_ctrl(HWND hwnd, int id, int x, int y, int w, int h) {
    HWND c = GetDlgItem(hwnd, id);
    /* bRepaint=FALSE: the parent does one InvalidateRect at end of layout */
    if (c) MoveWindow(c, x, y, w, h, FALSE);
}

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