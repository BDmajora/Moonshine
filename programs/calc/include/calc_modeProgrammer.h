#ifndef CALC_MODE_PROGRAMMER_H
#define CALC_MODE_PROGRAMMER_H

#include <windows.h>
#include "calc_defs.h"

/* ────────────────────────────────────────────────────────────────────
   calc_modeProgrammer.h — Programmer mode.

   Layout (matches Windows 7 Calculator):
     Top:  Bit-display strip (full width)
     Left: Hex/Dec/Oct/Bin radio group
           Qword/Dword/Word/Byte radio group (below, separated)
     Right: 8-column button grid, 6 rows

     Row 0 (right grid):   _   Mod  A   MC   MR  MS  M+  M-
     Row 1 (right grid):  (    )    B   ←    CE  C   ±   √
     Row 2 (right grid):  RoL  RoR  C   7    8   9   /   %
     Row 3 (right grid):  Or   Xor  D   4    5   6   ×   1/x
     Row 4 (right grid):  Lsh  Rsh  E   1    2   3   −   = (spans 2)
     Row 5 (right grid):  Not  And  F   0 (spans 2)  .   +
   ──────────────────────────────────────────────────────────────────── */

void mode_prog_create(HWND hwnd);
void mode_prog_layout(HWND hwnd, Layout *L);
void mode_prog_get_grid(int *cols, int *rows, int *header_h);

#endif