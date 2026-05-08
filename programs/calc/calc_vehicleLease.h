#ifndef CALC_VEHICLELEASE_H
#define CALC_VEHICLELEASE_H

#include <windows.h>

/* ────────────────────────────────────────────────────────────────────
   calc_vehicleLease.h — Vehicle Lease worksheet.

   Six variables total:
       Lease value       (solve-for #0, default)
       Lease period      (solve-for #1)
       Payments per year (always an input)
       Residual value    (solve-for #2)
       Interest rate (%) (always an input)
       Periodic payment  (solve-for #3)

   Combo offers four solve-for choices: Lease value, Lease period,
   Residual value, Periodic payment. Payments per year and Interest
   rate are always inputs — they're never the unknown.

   When the user picks a solve-for target, that variable becomes the
   output; the other five are shown as input fields in the panel.
   ──────────────────────────────────────────────────────────────────── */

void vlease_init_combo(HWND hwnd);
void vlease_refresh_fields(HWND hwnd);
void vlease_calculate(HWND hwnd);

#endif /* CALC_VEHICLELEASE_H */