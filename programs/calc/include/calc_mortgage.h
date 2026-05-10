#ifndef CALC_MORTGAGE_H
#define CALC_MORTGAGE_H

#include <windows.h>

/* ────────────────────────────────────────────────────────────────────
   calc_mortgage.h — single source of truth for the Mortgage worksheet.

   The mortgage worksheet has 5 variables:
       0  Monthly payment
       1  Purchase price
       2  Down payment
       3  Term (years)
       4  Interest rate (%)

   The user picks ONE to "solve for" via the worksheet combo box; the
   other four become input fields whose labels are set dynamically.
   This module owns:
     - The dropdown contents (mortgage_init_combo)
     - The label/field rearrangement (mortgage_refresh_fields)
     - The math (mortgage_calculate)

   Callers:
     calc_create.c     → mortgage_init_combo when building the panel
     calc_cmdDispatch.c → mortgage_refresh_fields on combo change,
                          mortgage_calculate on Calculate click
   ──────────────────────────────────────────────────────────────────── */

/* Populate the worksheet combo with the 5 solve-for options and
   reset the input labels to match the default selection. */
void mortgage_init_combo(HWND hwnd);

/* Refresh the input labels and visibility based on the current
   combo selection. Call this whenever the combo changes. */
void mortgage_refresh_fields(HWND hwnd);

/* Read the inputs, compute the missing variable, and write the
   formatted result into ID_WS_RES. */
void mortgage_calculate(HWND hwnd);

#endif /* CALC_MORTGAGE_H */