/* ────────────────────────────────────────────────────────────────────
   calc_vehicleLease.c — Vehicle Lease worksheet: combo, fields, math.

   Mirrors calc_mortgage.c in structure. The math model is the
   standard annuity formula with residual value:

       periodic_rate r = annual_rate / payments_per_year
       n = lease_period_years * payments_per_year
       PV  = lease_value
       FV  = residual_value
       Solve for periodic payment P from:
           PV = P * (1 - (1+r)^-n) / r  +  FV * (1+r)^-n

   For r == 0, the limiting case is:
           P = (PV - FV) / n

   Solving for any of the five other variables uses closed-form
   inversions (or Newton's method for interest rate).
   ──────────────────────────────────────────────────────────────────── */
#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <wchar.h>
#include <stdlib.h>
#include "calc_defs.h"
#include "calc_vehicleLease.h"

/* All six variables in canonical index order. The combo only
   exposes indices 0, 1, 3, 5 as solve-for choices; 2 and 4 are
   always inputs. */
enum {
    V_VALUE   = 0,   /* Lease value       */
    V_PERIOD  = 1,   /* Lease period (yr) */
    V_PPY     = 2,   /* Payments per year */
    V_RES     = 3,   /* Residual value    */
    V_RATE    = 4,   /* Interest rate (%) */
    V_PAYMENT = 5,   /* Periodic payment  */
    V_COUNT   = 6
};

static const WCHAR *v_labels[V_COUNT] = {
    L"Lease value",
    L"Lease period",
    L"Payments per year",
    L"Residual value",
    L"Interest rate (%)",
    L"Periodic payment"
};

/* Combo "solve for" choices, in display order. Each entry is an
   index into the v_labels[] / variable arrays above. The Windows 7
   reference shows them in this exact order. */
static const int combo_to_var[] = {
    V_PERIOD,    /* "Lease period"     */
    V_VALUE,     /* "Lease value"      */
    V_PAYMENT,   /* "Periodic payment" */
    V_RES        /* "Residual value"   */
};
#define COMBO_OPTION_COUNT 4
#define COMBO_DEFAULT_INDEX 2   /* "Periodic payment" — default */

static const int slot_lbl[6] = {
    ID_WS_LBL1, ID_WS_LBL2, ID_WS_LBL3,
    ID_WS_LBL4, ID_WS_LBL5, ID_WS_LBL6
};
static const int slot_inp[6] = {
    ID_WS_IN1, ID_WS_IN2, ID_WS_IN3,
    ID_WS_IN4, ID_WS_IN5, ID_WS_IN6
};

/* ── Helpers ─────────────────────────────────────────────────────── */

/* Returns the variable index currently being solved for. */
static int solve_var(HWND hwnd) {
    int sel = (int)SendMessageW(GetDlgItem(hwnd, ID_WS_COMBO),
                                CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= COMBO_OPTION_COUNT) sel = COMBO_DEFAULT_INDEX;
    return combo_to_var[sel];
}

static double field_get(HWND hwnd, int slot) {
    WCHAR buf[64];
    HWND h = GetDlgItem(hwnd, slot_inp[slot]);
    if (!h) return 0.0;
    GetWindowTextW(h, buf, 64);
    return wcstod(buf, NULL);
}

/* ── Public: populate the combo ──────────────────────────────────── */

void vlease_init_combo(HWND hwnd) {
    HWND hc = GetDlgItem(hwnd, ID_WS_COMBO);
    int i;
    if (!hc) return;
    SendMessageW(hc, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < COMBO_OPTION_COUNT; i++)
        SendMessageW(hc, CB_ADDSTRING, 0, (LPARAM)v_labels[combo_to_var[i]]);
    SendMessageW(hc, CB_SETCURSEL, COMBO_DEFAULT_INDEX, 0);
    vlease_refresh_fields(hwnd);
}

/* ── Public: rearrange input labels to match the combo ───────────── */

void vlease_refresh_fields(HWND hwnd) {
    int solve = solve_var(hwnd);
    int slot = 0, i;

    /* Show the 5 variables that aren't being solved for, in canonical
       order, packed into slots 0..4. */
    for (i = 0; i < V_COUNT; i++) {
        HWND hl, hi;
        if (i == solve) continue;
        hl = GetDlgItem(hwnd, slot_lbl[slot]);
        hi = GetDlgItem(hwnd, slot_inp[slot]);
        if (hl) {
            SetWindowTextW(hl, v_labels[i]);
            ShowWindow(hl, SW_SHOW);
        }
        if (hi) {
            SetWindowTextW(hi, L"");
            ShowWindow(hi, SW_SHOW);
        }
        slot++;
    }
    /* Hide unused slots (slot 5). */
    for (i = slot; i < 6; i++) {
        HWND hl = GetDlgItem(hwnd, slot_lbl[i]);
        HWND hi = GetDlgItem(hwnd, slot_inp[i]);
        if (hl) ShowWindow(hl, SW_HIDE);
        if (hi) ShowWindow(hi, SW_HIDE);
    }
    SetWindowTextW(GetDlgItem(hwnd, ID_WS_RES), L"");
}

/* ── Math helper: payment from the other 5 variables ─────────────── */
/* PV = P*(1-(1+r)^-n)/r + FV*(1+r)^-n   →  P = (PV - FV*(1+r)^-n) * r / (1-(1+r)^-n) */
static double calc_payment(double pv, double fv, double r, double n) {
    double pn;
    if (n <= 0) return 0;
    if (r == 0.0) return (pv - fv) / n;
    pn = pow(1.0 + r, -n);
    return (pv - fv * pn) * r / (1.0 - pn);
}

/* ── Public: solve and display ───────────────────────────────────── */

void vlease_calculate(HWND hwnd) {
    int solve = solve_var(hwnd);
    double vals[V_COUNT] = {0, 0, 0, 0, 0, 0};
    double value, period, ppy, res_val, rate, payment;
    double r, n, pn;
    WCHAR out[128];
    int slot = 0, i;

    /* Pull the 5 visible fields back into their canonical slots. */
    for (i = 0; i < V_COUNT; i++) {
        if (i == solve) continue;
        vals[i] = field_get(hwnd, slot++);
    }
    value   = vals[V_VALUE];
    period  = vals[V_PERIOD];
    ppy     = vals[V_PPY];
    res_val = vals[V_RES];
    rate    = vals[V_RATE];
    payment = vals[V_PAYMENT];

    if (ppy <= 0) ppy = 12;  /* sensible default */
    r = rate / 100.0 / ppy;
    n = period * ppy;

    out[0] = L'\0';

    switch (solve) {
        case V_PAYMENT: {
            if (n <= 0 || value <= 0) {
                wcscpy(out, L"Invalid inputs"); break;
            }
            payment = calc_payment(value, res_val, r, n);
            swprintf(out, 128, L"Periodic payment: $%.2f", payment);
            break;
        }

        case V_VALUE: {
            /* PV = P*(1-(1+r)^-n)/r + FV*(1+r)^-n */
            if (n <= 0 || payment <= 0) {
                wcscpy(out, L"Invalid inputs"); break;
            }
            if (r == 0.0)
                value = payment * n + res_val;
            else {
                pn = pow(1.0 + r, -n);
                value = payment * (1.0 - pn) / r + res_val * pn;
            }
            swprintf(out, 128, L"Lease value: $%.2f", value);
            break;
        }

        case V_RES: {
            /* FV = (PV - P*(1-(1+r)^-n)/r) / (1+r)^-n */
            if (n <= 0 || value <= 0) {
                wcscpy(out, L"Invalid inputs"); break;
            }
            if (r == 0.0)
                res_val = value - payment * n;
            else {
                pn = pow(1.0 + r, -n);
                res_val = (value - payment * (1.0 - pn) / r) / pn;
            }
            swprintf(out, 128, L"Residual value: $%.2f", res_val);
            break;
        }

        case V_PERIOD: {
            /* Solve PV = P*(1-(1+r)^-n)/r + FV*(1+r)^-n  for n,
               then period = n / ppy. */
            if (payment <= 0 || value <= 0 || ppy <= 0) {
                wcscpy(out, L"Invalid inputs"); break;
            }
            if (r == 0.0) {
                if (payment == 0) { wcscpy(out, L"Invalid inputs"); break; }
                n = (value - res_val) / payment;
            } else {
                /* Rearranges to: (1+r)^-n = (P/r - PV) / (P/r - FV) */
                double a = payment / r - value;
                double b = payment / r - res_val;
                double q;
                if (b == 0) { wcscpy(out, L"Cannot solve"); break; }
                q = a / b;
                if (q <= 0) { wcscpy(out, L"Cannot solve"); break; }
                n = -log(q) / log(1.0 + r);
            }
            period = n / ppy;
            if (period <= 0) {
                wcscpy(out, L"Cannot solve"); break;
            }
            swprintf(out, 128, L"Lease period: %.2f years", period);
            break;
        }

        default:
            wcscpy(out, L"Cannot solve for that variable");
    }
    SetWindowTextW(GetDlgItem(hwnd, ID_WS_RES), out);
}