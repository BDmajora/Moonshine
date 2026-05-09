/* ────────────────────────────────────────────────────────────────────
   calc_fuelEconomy.c — Fuel Economy worksheet: combo, fields, math.
   ──────────────────────────────────────────────────────────────────── */
#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <wchar.h>
#include <stdlib.h>
#include "calc_defs.h"
#include "calc_fuelEconomy.h"

enum {
    F_DISTANCE = 0,
    F_FUEL     = 1,
    F_ECONOMY  = 2,
    F_COUNT    = 3
};

static const WCHAR *labels_mpg[F_COUNT] = {
    L"Distance (miles)",
    L"Fuel used (gallons)",
    L"Fuel economy (mpg)"
};
static const WCHAR *labels_lkm[F_COUNT] = {
    L"Distance (kilometers)",
    L"Fuel used (liters)",
    L"Fuel economy (L/100 km)"
};

/* Combo display order, matching Windows 7 Calculator.
   The Windows reference shows: Distance, Fuel economy, Fuel used.
   Default selection is "Fuel economy" (middle option). */
static const int combo_order[F_COUNT] = {
    F_DISTANCE,  /* Distance (miles/km)       — first  */
    F_ECONOMY,   /* Fuel economy (mpg/L100km) — second, default */
    F_FUEL       /* Fuel used (gallons/liters)— third  */
};
#define COMBO_DEFAULT_INDEX 1  /* "Fuel economy" is the middle option */

static const int slot_lbl[6] = {
    ID_WS_LBL1, ID_WS_LBL2, ID_WS_LBL3,
    ID_WS_LBL4, ID_WS_LBL5, ID_WS_LBL6
};
static const int slot_inp[6] = {
    ID_WS_IN1, ID_WS_IN2, ID_WS_IN3,
    ID_WS_IN4, ID_WS_IN5, ID_WS_IN6
};

/* ── Helpers ─────────────────────────────────────────────────────── */

static const WCHAR **labels_for(BOOL is_lkm) {
    return is_lkm ? labels_lkm : labels_mpg;
}

static int solve_var(HWND hwnd) {
    int sel = (int)SendMessageW(GetDlgItem(hwnd, ID_WS_COMBO),
                                CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= F_COUNT) sel = COMBO_DEFAULT_INDEX;
    return combo_order[sel];  /* Map combo index → variable index */
}

static double field_get(HWND hwnd, int slot) {
    WCHAR buf[64];
    HWND h = GetDlgItem(hwnd, slot_inp[slot]);
    if (!h) return 0.0;
    GetWindowTextW(h, buf, 64);
    return wcstod(buf, NULL);
}

/* ── Public: populate the combo ──────────────────────────────────── */

void fuel_init_combo(HWND hwnd, BOOL is_lkm) {
    HWND hc = GetDlgItem(hwnd, ID_WS_COMBO);
    const WCHAR **labels = labels_for(is_lkm);
    int i;
    if (!hc) return;
    SendMessageW(hc, CB_RESETCONTENT, 0, 0);
    /* Add in Windows display order: Distance, Fuel economy, Fuel used */
    for (i = 0; i < F_COUNT; i++)
        SendMessageW(hc, CB_ADDSTRING, 0, (LPARAM)labels[combo_order[i]]);
    SendMessageW(hc, CB_SETCURSEL, COMBO_DEFAULT_INDEX, 0);
    fuel_refresh_fields(hwnd, is_lkm);
}

/* ── Public: rearrange input labels to match the combo ───────────── */

void fuel_refresh_fields(HWND hwnd, BOOL is_lkm) {
    int solve = solve_var(hwnd);
    const WCHAR **labels = labels_for(is_lkm);
    int slot = 0, i;

    /* Show the 2 variables that aren't being solved for. */
    for (i = 0; i < F_COUNT; i++) {
        HWND hl, hi;
        if (i == solve) continue;
        hl = GetDlgItem(hwnd, slot_lbl[slot]);
        hi = GetDlgItem(hwnd, slot_inp[slot]);
        if (hl) {
            SetWindowTextW(hl, labels[i]);
            ShowWindow(hl, SW_SHOW);
        }
        if (hi) {
            SetWindowTextW(hi, L"");
            ShowWindow(hi, SW_SHOW);
        }
        slot++;
    }
    /* Hide remaining slots (slots 2..5). */
    for (i = slot; i < 6; i++) {
        HWND hl = GetDlgItem(hwnd, slot_lbl[i]);
        HWND hi = GetDlgItem(hwnd, slot_inp[i]);
        if (hl) ShowWindow(hl, SW_HIDE);
        if (hi) ShowWindow(hi, SW_HIDE);
    }
    SetWindowTextW(GetDlgItem(hwnd, ID_WS_RES), L"");
}

/* ── Public: solve and display ───────────────────────────────────── */

void fuel_calculate(HWND hwnd, BOOL is_lkm) {
    int solve = solve_var(hwnd);
    const WCHAR **labels = labels_for(is_lkm);
    double vals[F_COUNT] = {0, 0, 0};
    double distance, fuel, economy, result;
    WCHAR out[128];
    int slot = 0, i;

    /* Pull the 2 visible fields back into their canonical slots. */
    for (i = 0; i < F_COUNT; i++) {
        if (i == solve) continue;
        vals[i] = field_get(hwnd, slot++);
    }
    distance = vals[F_DISTANCE];
    fuel     = vals[F_FUEL];
    economy  = vals[F_ECONOMY];

    out[0] = L'\0';
    result = 0;

    switch (solve) {
        case F_ECONOMY:
            if (is_lkm) {
                /* L/100 km = (fuel / distance) * 100 */
                if (distance <= 0) { wcscpy(out, L"Invalid inputs"); break; }
                result = (fuel / distance) * 100.0;
                swprintf(out, 128, L"%s: %.2f", labels[F_ECONOMY], result);
            } else {
                /* mpg = distance / fuel */
                if (fuel <= 0) { wcscpy(out, L"Invalid inputs"); break; }
                result = distance / fuel;
                swprintf(out, 128, L"%s: %.2f", labels[F_ECONOMY], result);
            }
            break;

        case F_FUEL:
            if (is_lkm) {
                /* fuel = (economy * distance) / 100 */
                if (distance <= 0 || economy < 0) {
                    wcscpy(out, L"Invalid inputs"); break;
                }
                result = (economy * distance) / 100.0;
            } else {
                /* fuel = distance / economy */
                if (economy <= 0) {
                    wcscpy(out, L"Invalid inputs"); break;
                }
                result = distance / economy;
            }
            swprintf(out, 128, L"%s: %.2f", labels[F_FUEL], result);
            break;

        case F_DISTANCE:
            if (is_lkm) {
                /* distance = (fuel * 100) / economy */
                if (economy <= 0) {
                    wcscpy(out, L"Invalid inputs"); break;
                }
                result = (fuel * 100.0) / economy;
            } else {
                /* distance = economy * fuel */
                if (fuel < 0 || economy < 0) {
                    wcscpy(out, L"Invalid inputs"); break;
                }
                result = economy * fuel;
            }
            swprintf(out, 128, L"%s: %.2f", labels[F_DISTANCE], result);
            break;
    }
    SetWindowTextW(GetDlgItem(hwnd, ID_WS_RES), out);
}