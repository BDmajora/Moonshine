#include "calc_input.h"
#include <wchar.h>

/* ── Input Handlers ──────────────────────────────────────────────── */

void handle_digit(HWND hwnd, WCHAR ch) {
    if (error) return;
    if (new_input) {
        display_str[0] = ch;
        display_str[1] = L'\0';
        new_input = FALSE;
    } else {
        size_t len = wcslen(display_str);
        if (len >= MAX_DISPLAY - 1) return;
        if (wcscmp(display_str, L"0") == 0 && ch != L'.') {
            display_str[0] = ch;
            display_str[1] = L'\0';
        } else {
            display_str[len]   = ch;
            display_str[len+1] = L'\0';
        }
    }
    update_ui_text(hwnd);
}

void handle_dot(HWND hwnd) {
    if (error) return;
    if (prog_base != BASE_DEC) return;  /* no decimals in non-decimal mode */
    if (new_input) {
        wcscpy(display_str, L"0.");
        new_input = FALSE;
    } else if (!wcschr(display_str, L'.')) {
        size_t len = wcslen(display_str);
        display_str[len]   = L'.';
        display_str[len+1] = L'\0';
    }
    update_ui_text(hwnd);
}

void handle_back(HWND hwnd) {
    size_t len;
    if (error || new_input) return;
    len = wcslen(display_str);
    if (len <= 1 || (len == 2 && display_str[0] == L'-')) {
        wcscpy(display_str, L"0");
        new_input = TRUE;
    } else {
        display_str[len - 1] = L'\0';
        if (wcscmp(display_str, L"-") == 0) {
            wcscpy(display_str, L"0");
            new_input = TRUE;
        }
    }
    update_ui_text(hwnd);
}

void handle_paren_open(HWND hwnd) {
    /* Simplified: just track depth for display purposes */
    paren_depth++;
    (void)hwnd;
}

void handle_paren_close(HWND hwnd) {
    if (paren_depth > 0) paren_depth--;
    (void)hwnd;
}