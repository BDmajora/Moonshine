#ifndef CALC_MODE_STANDARD_H
#define CALC_MODE_STANDARD_H

#include <windows.h>
#include "calc_defs.h"

/* ────────────────────────────────────────────────────────────────────
   calc_modeStandard.h — Standard calculator mode.

   5-col, 6-row layout:
     Row 0: MC MR MS M+ M-
     Row 1: ← CE C ± √
     Row 2: 7 8 9 / %
     Row 3: 4 5 6 × 1/x
     Row 4: 1 2 3 − = (spans 2 rows)
     Row 5: 0 0 .  +
   ──────────────────────────────────────────────────────────────────── */

void mode_std_create(HWND hwnd);
void mode_std_layout(HWND hwnd, Layout *L);
void mode_std_get_grid(int *cols, int *rows, int *header_h);

#endif