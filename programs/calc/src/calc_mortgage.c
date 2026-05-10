/* ────────────────────────────────────────────────────────────────────
   calc_mortgage.c — Mortgage worksheet: combo, fields, math.

   This is the ONLY file that knows about mortgage variables.
   Previously the data was split across calc_create.c (combo entries),
   calc_panel.c (label arrays + math), and calc_cmdDispatch.c (no
   combo handler at all — which is why changing the dropdown did
   nothing in earlier builds).
   ──────────────────────────────────────────────────────────────────── */
#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <wchar.h>
#include <stdlib.h>
#include "calc_defs.h"
#include "calc_mortgage.h"

/* The 5 mortgage variables, in canonical index order. The combo
   box is populated in this exact order, so the combo's CB_GETCURSEL
   maps directly to a variable index. */
enum {
    M_PAYMENT = 0,
    M_PRICE   = 1,
    M_DOWN    = 2,
    M_YEARS   = 3,
    M_RATE    = 4,
    M_COUNT   = 5
};

static const WCHAR *m_labels[M_COUNT] = {
    L"Monthly payment",
    L"Purchase price",
    L"Down payment",
    L"Term (years)",
    L"Interest rate (%)"
};

/* Map a "slot" (visible field index 0..3) to a control ID. */
static const int slot_lbl[6] = {
    ID_WS_LBL1, ID_WS_LBL2, ID_WS_LBL3,
    ID_WS_LBL4, ID_WS_LBL5, ID_WS_LBL6
};
static const int slot_inp[6] = {
    ID_WS_IN1, ID_WS_IN2, ID_WS_IN3,
    ID_WS_IN4, ID_WS_IN5, ID_WS_IN6
};

/* ── Helpers ─────────────────────────────────────────────────────── */

static int combo_get(HWND hwnd) {
    int sel = (int)SendMessageW(GetDlgItem(hwnd, ID_WS_COMBO),
                                CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= M_COUNT) sel = M_PAYMENT;
    return sel;
}

static double field_get(HWND hwnd, int slot) {
    WCHAR buf[64];
    HWND h = GetDlgItem(hwnd, slot_inp[slot]);
    if (!h) return 0.0;
    GetWindowTextW(h, buf, 64);
    return wcstod(buf, NULL);
}

/* ── Public: populate the combo ──────────────────────────────────── */

void mortgage_init_combo(HWND hwnd) {
    HWND hc = GetDlgItem(hwnd, ID_WS_COMBO);
    int i;
    if (!hc) return;
    SendMessageW(hc, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < M_COUNT; i++)
        SendMessageW(hc, CB_ADDSTRING, 0, (LPARAM)m_labels[i]);
    SendMessageW(hc, CB_SETCURSEL, M_PAYMENT, 0);
    mortgage_refresh_fields(hwnd);
}

/* ── Public: rearrange input labels to match the combo ───────────── */

void mortgage_refresh_fields(HWND hwnd) {
    int solve = combo_get(hwnd);
    int slot = 0, i;

    /* Show the 4 variables that aren't being solved for, in canonical
       order, packed into slots 0..3. */
    for (i = 0; i < M_COUNT; i++) {
        HWND hl, hi;
        if (i == solve) continue;
        hl = GetDlgItem(hwnd, slot_lbl[slot]);
        hi = GetDlgItem(hwnd, slot_inp[slot]);
        if (hl) {
            SetWindowTextW(hl, m_labels[i]);
            ShowWindow(hl, SW_SHOW);
        }
        if (hi) {
            SetWindowTextW(hi, L"");
            ShowWindow(hi, SW_SHOW);
        }
        slot++;
    }
    /* Hide unused slots (slot 4 and 5). */
    for (i = slot; i < 6; i++) {
        HWND hl = GetDlgItem(hwnd, slot_lbl[i]);
        HWND hi = GetDlgItem(hwnd, slot_inp[i]);
        if (hl) ShowWindow(hl, SW_HIDE);
        if (hi) ShowWindow(hi, SW_HIDE);
    }
    SetWindowTextW(GetDlgItem(hwnd, ID_WS_RES), L"");
}

/* ── Public: solve and display ───────────────────────────────────── */

void mortgage_calculate(HWND hwnd) {
    int solve = combo_get(hwnd);
    double vals[M_COUNT] = {0, 0, 0, 0, 0};
    double payment, price, down, years, rate;
    double r, n, pn, principal;
    WCHAR res[128];
    int slot = 0, i;

    /* Pull the 4 visible fields back into their canonical slots. */
    for (i = 0; i < M_COUNT; i++) {
        if (i == solve) continue;
        vals[i] = field_get(hwnd, slot++);
    }
    payment = vals[M_PAYMENT];
    price   = vals[M_PRICE];
    down    = vals[M_DOWN];
    years   = vals[M_YEARS];
    rate    = vals[M_RATE];

    r = rate / 100.0 / 12.0;   /* monthly rate    */
    n = years * 12.0;          /* months          */

    res[0] = L'\0';

    switch (solve) {
        case M_PAYMENT:
            principal = price - down;
            if (principal <= 0 || n <= 0) {
                wcscpy(res, L"Invalid inputs");
                break;
            }
            if (r == 0.0) payment = principal / n;
            else {
                pn = pow(1 + r, n);
                payment = principal * r * pn / (pn - 1.0);
            }
            swprintf(res, 128, L"Monthly payment: $%.2f", payment);
            break;

        case M_PRICE:
            if (n <= 0 || payment <= 0) {
                wcscpy(res, L"Invalid inputs");
                break;
            }
            if (r == 0.0) price = payment * n + down;
            else {
                pn = pow(1 + r, n);
                principal = payment * (pn - 1.0) / (r * pn);
                price = principal + down;
            }
            swprintf(res, 128, L"Purchase price: $%.2f", price);
            break;

        case M_DOWN:
            if (n <= 0 || payment <= 0 || price <= 0) {
                wcscpy(res, L"Invalid inputs");
                break;
            }
            if (r == 0.0) principal = payment * n;
            else {
                pn = pow(1 + r, n);
                principal = payment * (pn - 1.0) / (r * pn);
            }
            down = price - principal;
            swprintf(res, 128, L"Down payment: $%.2f", down);
            break;

        case M_YEARS: {
            principal = price - down;
            if (principal <= 0 || payment <= 0) {
                wcscpy(res, L"Invalid inputs");
                break;
            }
            if (r == 0.0) {
                years = principal / payment / 12.0;
            } else {
                /* n = -ln(1 - principal*r/payment) / ln(1+r) */
                double x = 1.0 - principal * r / payment;
                if (x <= 0) {
                    wcscpy(res, L"Payment too low to ever repay");
                    break;
                }
                n = -log(x) / log(1.0 + r);
                years = n / 12.0;
            }
            swprintf(res, 128, L"Term: %.2f years", years);
            break;
        }

        case M_RATE: {
            /* Newton's method on f(r) = principal*r*(1+r)^n / ((1+r)^n - 1) - payment */
            double guess = 0.005;  /* 6% APR / 12 = 0.5% monthly */
            int iter;
            principal = price - down;
            if (principal <= 0 || payment <= 0 || n <= 0) {
                wcscpy(res, L"Invalid inputs");
                break;
            }
            if (payment * n <= principal) {
                wcscpy(res, L"Payment too low; rate would be negative");
                break;
            }
            for (iter = 0; iter < 200; iter++) {
                double pg, f, h, pg2, f2, dp;
                pg = pow(1 + guess, n);
                f  = principal * guess * pg / (pg - 1.0) - payment;
                h  = 1e-8;
                pg2 = pow(1 + guess + h, n);
                f2  = principal * (guess + h) * pg2 / (pg2 - 1.0) - payment;
                dp = (f2 - f) / h;
                if (fabs(dp) < 1e-15) break;
                guess -= f / dp;
                if (guess < 1e-9) guess = 1e-9;
                if (fabs(f) < 1e-4) break;
            }
            rate = guess * 12.0 * 100.0;
            swprintf(res, 128, L"Interest rate: %.3f%%", rate);
            break;
        }
    }
    SetWindowTextW(GetDlgItem(hwnd, ID_WS_RES), res);
}