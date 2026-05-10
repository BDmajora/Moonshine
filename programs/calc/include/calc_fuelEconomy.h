#ifndef CALC_FUELECONOMY_H
#define CALC_FUELECONOMY_H

#include <windows.h>

/* ────────────────────────────────────────────────────────────────────
   calc_fuelEconomy.h — Fuel Economy worksheet (mpg AND L/100 km).

   Three variables:
       Distance      (miles or kilometers)
       Fuel used     (gallons or liters)
       Fuel economy  (mpg — higher better, OR L/100 km — lower better)

   Any of the three can be the solve-for target.  The two variants
   (WS_FUEL_MPG vs WS_FUEL_LKM) share the same module since they
   differ only in unit labels and the economy formula direction:

       mpg:        economy = distance / fuel
       L/100 km:   economy = (fuel / distance) * 100
   ──────────────────────────────────────────────────────────────────── */

/* is_lkm: TRUE for L/100 km variant, FALSE for mpg. */
void fuel_init_combo(HWND hwnd, BOOL is_lkm);
void fuel_refresh_fields(HWND hwnd, BOOL is_lkm);
void fuel_calculate(HWND hwnd, BOOL is_lkm);

#endif /* CALC_FUELECONOMY_H */