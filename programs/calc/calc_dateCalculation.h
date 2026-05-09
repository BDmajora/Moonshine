#ifndef CALC_DATECALCULATION_H
#define CALC_DATECALCULATION_H

#include <windows.h>

/* ────────────────────────────────────────────────────────────────────
   calc_dateCalculation.h — Date Calculation panel.

   Two distinct modes selected from a dropdown:

     Mode 0 — "Calculate the difference between two dates"
       Two DateTimePickers (From / To)
       Two result labels:
         "Difference (years, months, weeks, days)"
         "Difference (days)"

     Mode 1 — "Add or subtract days to a specified date"
       One DateTimePicker (From)
       Two radio buttons (Add / Subtract)
       Three spinner inputs (Years / Months / Days)
       One result label ("Date")

   The two modes have completely different controls and layout, so
   the panel rebuilds itself when the mode combo changes (similar to
   how worksheet labels rearrange on solve-for change).
   ──────────────────────────────────────────────────────────────────── */

/* Build the controls for the panel.  Called once when the panel
   opens.  Internally calls date_apply_mode for the default mode. */
void date_create_controls(HWND hwnd);

/* Apply the layout for the current combo selection.  Hides controls
   that don't belong to the active mode and shows the ones that do.
   Called from layout_panel and from the combo CBN_SELCHANGE handler. */
void date_apply_mode(HWND hwnd);

/* Position controls based on current mode. Called from layout_panel. */
void date_layout(HWND hwnd, int px, int py, int pw, int gap);

/* Run the calculation. Called from the Calculate button. */
void date_calculate(HWND hwnd);

/* Called when the date-mode combo selection changes.
   Rebuilds visibility for the new mode. */
void date_mode_changed(HWND hwnd);

#endif /* CALC_DATECALCULATION_H */