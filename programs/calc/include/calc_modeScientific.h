#ifndef CALC_MODE_SCIENTIFIC_H
#define CALC_MODE_SCIENTIFIC_H

#include <windows.h>
#include "calc_defs.h"

/* ────────────────────────────────────────────────────────────────────
   calc_modeScientific.h — Scientific mode.

   10-col, 6-row layout. Top row shares: 5 radio-width cols for the
   Degrees/Radians/Grads radio group, then 5 cols for MC..M-.

     Row 0: [Degrees Radians Grads (spans 5)]  MC  MR  MS  M+  M-
     Row 1: _    Inv  ln  (   )    ←  CE  C   ±   √
     Row 2: Int  sinh sin x²  n!   7  8   9   /   %
     Row 3: dms  cosh cos xʸ  ʸ√x  4  5   6   ×   1/x
     Row 4: π    tanh tan x³  ³√x  1  2   3   −   = (spans 2 rows)
     Row 5: F-E  Exp  Mod log 10ˣ  0  (spans 2)  .  +
   ──────────────────────────────────────────────────────────────────── */

void mode_sci_create(HWND hwnd);
void mode_sci_layout(HWND hwnd, Layout *L);
void mode_sci_get_grid(int *cols, int *rows, int *header_h);

#endif