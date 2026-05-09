/* ────────────────────────────────────────────────────────────────────
   calc_dateCalculation.c — Date Calculation panel.

   Two modes with completely different controls:

     Mode 0 (DIFF):
       From DateTimePicker, To DateTimePicker
       Difference (years, months, weeks, days) — result line 1
       Difference (days)                       — result line 2

     Mode 1 (ADJUST):
       From DateTimePicker
       Add / Subtract radio buttons
       Year(s) / Month(s) / Day(s) spinners
       Date — result

   Implementation strategy: create ALL controls up front, then in
   date_apply_mode hide the ones that don't belong to the active
   mode. Layout positions only the visible controls.
   ──────────────────────────────────────────────────────────────────── */
#include <windows.h>
#include <commctrl.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include "calc_defs.h"
#include "calc_dateCalculation.h"

/* External widget factory (calc_widgets.c) */
extern void ui_register_child(HWND h);
extern HWND make_label(HWND p, int id, const WCHAR *text);
extern HWND make_combo(HWND p, int id);
extern HWND make_radio(HWND p, int id, const WCHAR *lbl, BOOL first);
extern HWND make_button(HWND p, int id, const WCHAR *lbl, BOOL def);
extern void move_ctrl(HWND hwnd, int id, int x, int y, int w, int h);

#define DATE_MODE_DIFF   0
#define DATE_MODE_ADJUST 1

/* ── Helpers ─────────────────────────────────────────────────────── */

static long long date_to_jdn(int y, int m, int d) {
    long long a = (14 - m) / 12;
    long long Y = y + 4800 - a;
    long long M = m + 12 * a - 3;
    return d + (153 * M + 2) / 5 + 365 * Y + Y / 4 - Y / 100 + Y / 400 - 32045;
}

static void jdn_to_date(long long jdn, int *y, int *m, int *d) {
    long long l, n, i, j;
    l = jdn + 68569;
    n = (4 * l) / 146097;
    l = l - (146097 * n + 3) / 4;
    i = (4000 * (l + 1)) / 1461001;
    l = l - (1461 * i) / 4 + 31;
    j = (80 * l) / 2447;
    *d = (int)(l - (2447 * j) / 80);
    l = j / 11;
    *m = (int)(j + 2 - 12 * l);
    *y = (int)(100 * (n - 49) + i + l);
}

static int current_mode(HWND hwnd) {
    int sel = (int)SendMessageW(GetDlgItem(hwnd, ID_DATE_TYPE),
                                CB_GETCURSEL, 0, 0);
    return (sel == DATE_MODE_ADJUST) ? DATE_MODE_ADJUST : DATE_MODE_DIFF;
}

static void show_id(HWND hwnd, int id, BOOL show) {
    HWND h = GetDlgItem(hwnd, id);
    if (h) ShowWindow(h, show ? SW_SHOW : SW_HIDE);
}

/* Read a SYSTEMTIME from a DateTimePicker. */
static void dtp_get(HWND hwnd, int id, SYSTEMTIME *st) {
    HWND h = GetDlgItem(hwnd, id);
    if (h) SendMessageW(h, DTM_GETSYSTEMTIME, 0, (LPARAM)st);
}

/* Read an integer from a spinner-attached edit. */
static int spin_get(HWND hwnd, int edit_id) {
    WCHAR buf[32];
    GetWindowTextW(GetDlgItem(hwnd, edit_id), buf, 32);
    return _wtoi(buf);
}

/* ── Public: create all controls (mode-independent) ──────────────── */

void date_create_controls(HWND hwnd) {
    SYSTEMTIME st;
    HWND h;

    GetLocalTime(&st);

    /* Title label + mode combo (always visible) */
    make_label(hwnd, 353, L"Select the date calculation you want");
    make_combo(hwnd, ID_DATE_TYPE);
    {
        HWND hc = GetDlgItem(hwnd, ID_DATE_TYPE);
        SendMessageW(hc, CB_RESETCONTENT, 0, 0);
        SendMessageW(hc, CB_ADDSTRING, 0,
            (LPARAM)L"Calculate the difference between two dates");
        SendMessageW(hc, CB_ADDSTRING, 0,
            (LPARAM)L"Add or subtract days to a specified date");
        SendMessageW(hc, CB_SETCURSEL, DATE_MODE_DIFF, 0);
    }

    /* "From" label — used in BOTH modes */
    make_label(hwnd, 354, L"From");

    /* DateTimePicker for "From" */
    h = CreateWindowW(DATETIMEPICK_CLASSW, NULL,
                      WS_CHILD | WS_VISIBLE | WS_BORDER |
                      DTS_SHORTDATEFORMAT,
                      0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_DATE_FROM,
                      NULL, NULL);
    ui_register_child(h);
    if (h) SendMessageW(h, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);

    /* "To" label + DateTimePicker — DIFF mode only */
    make_label(hwnd, 355, L"To");
    h = CreateWindowW(DATETIMEPICK_CLASSW, NULL,
                      WS_CHILD | WS_VISIBLE | WS_BORDER |
                      DTS_SHORTDATEFORMAT,
                      0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_DATE_TO,
                      NULL, NULL);
    ui_register_child(h);
    if (h) SendMessageW(h, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);

    /* Difference result fields — DIFF mode only.
       These are read-only EDIT controls (not STATIC labels) so they
       render with a visible sunken border, matching the Windows
       reference where the result appears in a gray box. */
    make_label(hwnd, ID_DATE_DIFF_LBL1, L"Difference (years, months, weeks, days)");
    h = CreateWindowW(L"EDIT", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER |
                      ES_LEFT | ES_READONLY | ES_AUTOHSCROLL,
                      0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_DATE_RES1,
                      NULL, NULL);
    ui_register_child(h);

    make_label(hwnd, ID_DATE_DIFF_LBL2, L"Difference (days)");
    h = CreateWindowW(L"EDIT", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER |
                      ES_LEFT | ES_READONLY | ES_AUTOHSCROLL,
                      0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_DATE_RES2,
                      NULL, NULL);
    ui_register_child(h);

    /* Add/Subtract radios — ADJUST mode only */
    make_radio(hwnd, ID_DATE_RAD_ADD, L"Add",      TRUE);
    make_radio(hwnd, ID_DATE_RAD_SUB, L"Subtract", FALSE);
    SendMessageW(GetDlgItem(hwnd, ID_DATE_RAD_ADD), BM_SETCHECK, BST_CHECKED, 0);

    /* Year(s) / Month(s) / Day(s) spinners — ADJUST mode only.
       Each is an edit control with a UpDown (spinner) buddy. */
    {
        static const struct { int lbl_id; const WCHAR *lbl; int edit_id; int spin_id; int max; } spins[] = {
            { ID_DATE_LBL_YEAR,  L"Year(s)",  ID_DATE_YEARS,  ID_DATE_YEARS_SPIN,  120  },
            { ID_DATE_LBL_MONTH, L"Month(s)", ID_DATE_MONTHS, ID_DATE_MONTHS_SPIN, 1200 },
            { ID_DATE_LBL_DAY,   L"Day(s)",   ID_DATE_DAYS,   ID_DATE_DAYS_SPIN,   36500},
        };
        int i;
        for (i = 0; i < 3; i++) {
            HWND he, hu;
            make_label(hwnd, spins[i].lbl_id, spins[i].lbl);
            he = CreateWindowW(L"EDIT", L"0",
                               WS_CHILD | WS_VISIBLE | WS_BORDER |
                               ES_NUMBER | ES_RIGHT,
                               0, 0, 0, 0, hwnd,
                               (HMENU)(INT_PTR)spins[i].edit_id, NULL, NULL);
            ui_register_child(he);
            hu = CreateWindowW(UPDOWN_CLASSW, NULL,
                               WS_CHILD | WS_VISIBLE |
                               UDS_SETBUDDYINT | UDS_ALIGNRIGHT |
                               UDS_ARROWKEYS | UDS_NOTHOUSANDS,
                               0, 0, 0, 0, hwnd,
                               (HMENU)(INT_PTR)spins[i].spin_id, NULL, NULL);
            ui_register_child(hu);
            if (hu) {
                SendMessageW(hu, UDM_SETBUDDY, (WPARAM)he, 0);
                SendMessageW(hu, UDM_SETRANGE32, 0, spins[i].max);
                SendMessageW(hu, UDM_SETPOS32, 0, 0);
            }
        }
    }

    /* "Date" result label + read-only edit — ADJUST mode only */
    make_label(hwnd, ID_DATE_DATE_LBL, L"Date");
    h = CreateWindowW(L"EDIT", L"",
                      WS_CHILD | WS_VISIBLE | WS_BORDER |
                      ES_LEFT | ES_READONLY | ES_AUTOHSCROLL,
                      0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_DATE_DATE_RES,
                      NULL, NULL);
    ui_register_child(h);

    /* Calculate button — both modes */
    make_button(hwnd, ID_DATE_CALC, L"Calculate", TRUE);

    /* Show/hide based on default mode */
    date_apply_mode(hwnd);
}

/* ── Public: show/hide controls for the active mode ──────────────── */

void date_apply_mode(HWND hwnd) {
    int mode = current_mode(hwnd);
    BOOL diff   = (mode == DATE_MODE_DIFF);
    BOOL adjust = (mode == DATE_MODE_ADJUST);

    /* Always visible: title, combo, From label, From DTP, Calculate */
    show_id(hwnd, 353,           TRUE);
    show_id(hwnd, ID_DATE_TYPE,  TRUE);
    show_id(hwnd, 354,           TRUE);
    show_id(hwnd, ID_DATE_FROM,  TRUE);
    show_id(hwnd, ID_DATE_CALC,  TRUE);

    /* DIFF-only */
    show_id(hwnd, 355,                diff);
    show_id(hwnd, ID_DATE_TO,         diff);
    show_id(hwnd, ID_DATE_DIFF_LBL1,  diff);
    show_id(hwnd, ID_DATE_RES1,       diff);
    show_id(hwnd, ID_DATE_DIFF_LBL2,  diff);
    show_id(hwnd, ID_DATE_RES2,       diff);

    /* ADJUST-only */
    show_id(hwnd, ID_DATE_RAD_ADD,    adjust);
    show_id(hwnd, ID_DATE_RAD_SUB,    adjust);
    show_id(hwnd, ID_DATE_LBL_YEAR,   adjust);
    show_id(hwnd, ID_DATE_YEARS,      adjust);
    show_id(hwnd, ID_DATE_YEARS_SPIN, adjust);
    show_id(hwnd, ID_DATE_LBL_MONTH,  adjust);
    show_id(hwnd, ID_DATE_MONTHS,     adjust);
    show_id(hwnd, ID_DATE_MONTHS_SPIN,adjust);
    show_id(hwnd, ID_DATE_LBL_DAY,    adjust);
    show_id(hwnd, ID_DATE_DAYS,       adjust);
    show_id(hwnd, ID_DATE_DAYS_SPIN,  adjust);
    show_id(hwnd, ID_DATE_DATE_LBL,   adjust);
    show_id(hwnd, ID_DATE_DATE_RES,   adjust);
}

void date_mode_changed(HWND hwnd) {
    date_apply_mode(hwnd);
    /* Trigger a layout pass on the panel area so the new controls
       get positioned correctly. */
    InvalidateRect(hwnd, NULL, TRUE);
}

/* ── Public: position controls within the panel ──────────────────── */

void date_layout(HWND hwnd, int px, int py, int pw, int gap) {
    int mode = current_mode(hwnd);
    int lh   = 18;     /* label height        */
    int eh   = 22;     /* edit/DTP height     */
    int bh2  = 28;     /* button height       */
    int lbl_w = 50;    /* "From"/"To" label width */
    int g    = gap;
    int y    = py;

    /* Title + combo (both modes) */
    move_ctrl(hwnd, 353,          px, y, pw, lh); y += lh + g;
    move_ctrl(hwnd, ID_DATE_TYPE, px, y, pw, eh); y += eh + g*2;

    if (mode == DATE_MODE_DIFF) {
        int half = (pw - g) / 2;
        int dtp_w = half - lbl_w - g;

        /* From / To row */
        move_ctrl(hwnd, 354,          px,                              y, lbl_w, eh);
        move_ctrl(hwnd, ID_DATE_FROM, px + lbl_w + g,                  y, dtp_w, eh);
        move_ctrl(hwnd, 355,          px + half + g,                   y, lbl_w, eh);
        move_ctrl(hwnd, ID_DATE_TO,   px + half + g + lbl_w + g,       y, dtp_w, eh);
        y += eh + g*2;

        /* Difference (years, months, weeks, days) */
        move_ctrl(hwnd, ID_DATE_DIFF_LBL1, px, y, pw, lh); y += lh + g;
        move_ctrl(hwnd, ID_DATE_RES1,      px, y, pw, eh); y += eh + g*2;

        /* Difference (days) */
        move_ctrl(hwnd, ID_DATE_DIFF_LBL2, px, y, pw, lh); y += lh + g;
        move_ctrl(hwnd, ID_DATE_RES2,      px, y, pw, eh); y += eh + g*2;

        /* Calculate — bottom-right */
        move_ctrl(hwnd, ID_DATE_CALC, px + pw - 100, y, 100, bh2);
    } else {
        /* ADJUST mode */
        int dtp_w   = 110;
        int radio_w = 80;
        int spin_lbl_w = 60;
        int spin_w  = (pw - spin_lbl_w*3 - g*5) / 3;
        int third;

        if (spin_w < 40) spin_w = 40;

        /* From + radios on same row */
        move_ctrl(hwnd, 354,              px,                           y, lbl_w, eh);
        move_ctrl(hwnd, ID_DATE_FROM,     px + lbl_w + g,               y, dtp_w, eh);
        move_ctrl(hwnd, ID_DATE_RAD_ADD,  px + lbl_w + g + dtp_w + g*2, y, radio_w, eh);
        move_ctrl(hwnd, ID_DATE_RAD_SUB,  px + lbl_w + g + dtp_w + g*2 + radio_w + g, y, radio_w, eh);
        y += eh + g*2;

        /* Year / Month / Day row — three label+spinner pairs */
        third = (pw - g*2) / 3;
        {
            static const int lbl_ids[]  = {ID_DATE_LBL_YEAR, ID_DATE_LBL_MONTH, ID_DATE_LBL_DAY};
            static const int edit_ids[] = {ID_DATE_YEARS, ID_DATE_MONTHS, ID_DATE_DAYS};
            int i;
            for (i = 0; i < 3; i++) {
                int x = px + (third + g) * i;
                move_ctrl(hwnd, lbl_ids[i],  x,                  y, spin_lbl_w, eh);
                move_ctrl(hwnd, edit_ids[i], x + spin_lbl_w + g, y, third - spin_lbl_w - g, eh);
                /* spinner positions itself via UDS_ALIGNRIGHT on its buddy edit */
            }
        }
        y += eh + g*2;

        /* Date result */
        move_ctrl(hwnd, ID_DATE_DATE_LBL, px, y, pw, lh); y += lh + g;
        move_ctrl(hwnd, ID_DATE_DATE_RES, px, y, pw, eh); y += eh + g*2;

        /* Calculate — bottom-right */
        move_ctrl(hwnd, ID_DATE_CALC, px + pw - 100, y, 100, bh2);
    }
}

/* ── Public: run the calculation ─────────────────────────────────── */

static void format_diff_yrmwd(long long days, WCHAR *out, int n) {
    long long abs_days;
    int years, rem, months, weeks, rdays;
    abs_days = days < 0 ? -days : days;
    years  = (int)(abs_days / 365);
    rem    = (int)(abs_days % 365);
    months = rem / 30;
    rem    = rem % 30;
    weeks  = rem / 7;
    rdays  = rem % 7;
    swprintf(out, n, L"%d years, %d months, %d weeks, %d days",
             years, months, weeks, rdays);
}

void date_calculate(HWND hwnd) {
    int mode = current_mode(hwnd);

    if (mode == DATE_MODE_DIFF) {
        SYSTEMTIME stFrom, stTo;
        long long jdnFrom, jdnTo, diff;
        WCHAR line1[128], line2[64];

        dtp_get(hwnd, ID_DATE_FROM, &stFrom);
        dtp_get(hwnd, ID_DATE_TO,   &stTo);

        jdnFrom = date_to_jdn(stFrom.wYear, stFrom.wMonth, stFrom.wDay);
        jdnTo   = date_to_jdn(stTo.wYear,   stTo.wMonth,   stTo.wDay);
        diff    = jdnTo - jdnFrom;

        format_diff_yrmwd(diff, line1, 128);
        swprintf(line2, 64, L"%lld", diff < 0 ? -diff : diff);

        SetWindowTextW(GetDlgItem(hwnd, ID_DATE_RES1), line1);
        SetWindowTextW(GetDlgItem(hwnd, ID_DATE_RES2), line2);
    } else {
        SYSTEMTIME stFrom;
        long long jdn;
        int years, months, days;
        BOOL subtract;
        int ry, rm, rd;
        WCHAR out[64];
        int new_y, new_m, new_d;

        dtp_get(hwnd, ID_DATE_FROM, &stFrom);
        years    = spin_get(hwnd, ID_DATE_YEARS);
        months   = spin_get(hwnd, ID_DATE_MONTHS);
        days     = spin_get(hwnd, ID_DATE_DAYS);
        subtract = (SendMessageW(GetDlgItem(hwnd, ID_DATE_RAD_SUB),
                                 BM_GETCHECK, 0, 0) == BST_CHECKED);

        if (subtract) { years = -years; months = -months; days = -days; }

        /* Apply years and months first (to JDN via reconstructed date),
           then add days. */
        new_y = stFrom.wYear  + years;
        new_m = stFrom.wMonth + months;
        new_d = stFrom.wDay;
        /* Normalize month overflow/underflow */
        while (new_m > 12) { new_m -= 12; new_y++; }
        while (new_m < 1)  { new_m += 12; new_y--; }

        jdn = date_to_jdn(new_y, new_m, new_d) + days;
        jdn_to_date(jdn, &ry, &rm, &rd);

        swprintf(out, 64, L"%04d-%02d-%02d", ry, rm, rd);
        SetWindowTextW(GetDlgItem(hwnd, ID_DATE_DATE_RES), out);
    }
}