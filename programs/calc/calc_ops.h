#ifndef CALC_OPS_H
#define CALC_OPS_H

#include <windows.h>
#include "calc_state.h"

/* ── Core operations ─────────────────────────────────────────────── */
void do_equals(HWND hwnd);
void handle_op(HWND hwnd, int new_op);

#endif /* CALC_OPS_H */