#ifndef CALC_MODE_STATISTICS_H
#define CALC_MODE_STATISTICS_H

#include <windows.h>
#include "calc_defs.h"

/* ────────────────────────────────────────────────────────────────────
   calc_modeStatistics.h — Statistics mode.

   Matches Windows 7 Calculator:
     - Listbox at the top (full width minus arrows column)
     - ▲ / ▼ buttons in the top-right corner
     - Main display BELOW the listbox
     - 5-col × 6-row button grid

     Row 0:  MC   MR   MS   M+   M-
     Row 1:  ←    CAD  C    F-E  Exp
     Row 2:  7    8    9    x̄    x̄²
     Row 3:  4    5    6    Σx   Σx²
     Row 4:  1    2    3    σn   σn-1
     Row 5:  0    .    ±    Add (spans 2)
   ──────────────────────────────────────────────────────────────────── */

void mode_stat_create(HWND hwnd);
void mode_stat_layout(HWND hwnd, Layout *L);
void mode_stat_get_grid(int *cols, int *rows, int *header_h);

#endif