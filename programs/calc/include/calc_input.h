#ifndef CALC_INPUT_H
#define CALC_INPUT_H

#include <windows.h>
#include "calc_state.h"

/* ── Input Handlers ──────────────────────────────────────────────── */
void handle_digit(HWND hwnd, WCHAR ch);
void handle_dot(HWND hwnd);
void handle_back(HWND hwnd);
void handle_paren_open(HWND hwnd);
void handle_paren_close(HWND hwnd);

#endif /* CALC_INPUT_H */