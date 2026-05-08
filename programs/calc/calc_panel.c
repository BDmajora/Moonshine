#include "calc_panel.h"
#include "calc_logic.h"
#include "calc_ui.h"
#include <math.h>
#include <wchar.h>
#include <stdlib.h>
#include <commctrl.h>

/* ════════════════════════════════════════════════════════════════════
   UNIT CONVERSION DATA
   ════════════════════════════════════════════════════════════════════ */

static const UnitEntry u_angle[] = {
    {L"Degree", 1.0}, {L"Radian", 57.29577951}, {L"Gradian", 0.9}
};
static const UnitEntry u_area[] = {
    {L"Square metre", 1.0}, {L"Square kilometre", 1e6},
    {L"Square foot", 0.09290304}, {L"Square mile", 2589988.11},
    {L"Acre", 4046.8564}, {L"Hectare", 10000.0}
};
static const UnitEntry u_energy[] = {
    {L"Joule", 1.0}, {L"Kilojoule", 1000.0}, {L"Calorie", 4.1868},
    {L"Kilocalorie", 4186.8}, {L"BTU", 1055.05587}, {L"Kilowatt-hour", 3600000.0},
    {L"Electronvolt", 1.60218e-19}
};
static const UnitEntry u_length[] = {
    {L"Metre", 1.0}, {L"Kilometre", 1000.0}, {L"Centimetre", 0.01},
    {L"Millimetre", 0.001}, {L"Mile", 1609.344}, {L"Yard", 0.9144},
    {L"Foot", 0.3048}, {L"Inch", 0.0254}, {L"Nautical mile", 1852.0},
    {L"Light year", 9.4607e15}
};
static const UnitEntry u_power[] = {
    {L"Watt", 1.0}, {L"Kilowatt", 1000.0}, {L"Horsepower", 745.69987},
    {L"BTU/minute", 17.5843}
};
static const UnitEntry u_pressure[] = {
    {L"Pascal", 1.0}, {L"Kilopascal", 1000.0}, {L"Bar", 100000.0},
    {L"Atmosphere", 101325.0}, {L"PSI", 6894.757}, {L"mmHg", 133.322}
};
static const UnitEntry u_temp[] = {
    {L"Celsius", 1.0}, {L"Fahrenheit", 1.0}, {L"Kelvin", 1.0}
};
static const UnitEntry u_time[] = {
    {L"Second", 1.0}, {L"Minute", 60.0}, {L"Hour", 3600.0},
    {L"Day", 86400.0}, {L"Week", 604800.0}, {L"Year", 31557600.0}
};
static const UnitEntry u_velocity[] = {
    {L"Metre/second", 1.0}, {L"Kilometre/hour", 0.277778},
    {L"Mile/hour", 0.44704}, {L"Knot", 0.514444}, {L"Mach", 340.29}
};
static const UnitEntry u_volume[] = {
    {L"Litre", 1.0}, {L"Millilitre", 0.001}, {L"Cubic metre", 1000.0},
    {L"US gallon", 3.785411784}, {L"UK gallon", 4.54609},
    {L"Fluid ounce (US)", 0.0295735}, {L"Cup (US)", 0.2365882},
    {L"Pint (US)", 0.473176}, {L"Cubic foot", 28.3168}
};
static const UnitEntry u_weight[] = {
    {L"Kilogram", 1.0}, {L"Gram", 0.001}, {L"Milligram", 1e-6},
    {L"Pound", 0.45359237}, {L"Ounce", 0.0283495}, {L"Stone", 6.35029},
    {L"Tonne", 1000.0}, {L"US ton", 907.185}
};

const UnitCategory g_unit_cats[] = {
    {L"Angle",    3,  u_angle},
    {L"Area",     6,  u_area},
    {L"Energy",   7,  u_energy},
    {L"Length",  10,  u_length},
    {L"Power",    4,  u_power},
    {L"Pressure", 6,  u_pressure},
    {L"Temperature", 3, u_temp},
    {L"Time",     6,  u_time},
    {L"Velocity", 5,  u_velocity},
    {L"Volume",   9,  u_volume},
    {L"Weight",   8,  u_weight}
};
const int g_unit_cat_count = 11;

/* ── Temperature special conversion ───────────────────────────────── */
static double temp_to_celsius(double v, int idx) {
    if (idx == 1) return (v - 32.0) * 5.0 / 9.0; /* Fahrenheit */
    if (idx == 2) return v - 273.15;               /* Kelvin */
    return v;
}
static double celsius_to_unit(double c, int idx) {
    if (idx == 1) return c * 9.0 / 5.0 + 32.0;
    if (idx == 2) return c + 273.15;
    return c;
}

/* ── Panel helpers ───────────────────────────────────────────────── */
static HWND ctrl(HWND hwnd, int id) { return GetDlgItem(hwnd, id); }

static void set_ctrl_text(HWND hwnd, int id, const WCHAR *s) {
    SetWindowTextW(ctrl(hwnd, id), s);
}

static void get_ctrl_text(HWND hwnd, int id, WCHAR *buf, int n) {
    GetWindowTextW(ctrl(hwnd, id), buf, n);
}

static int get_combo_sel(HWND hwnd, int id) {
    return (int)SendMessageW(ctrl(hwnd, id), CB_GETCURSEL, 0, 0);
}

static void combo_add(HWND hwnd, int id, const WCHAR *s) {
    SendMessageW(ctrl(hwnd, id), CB_ADDSTRING, 0, (LPARAM)s);
}

static void combo_sel(HWND hwnd, int id, int i) {
    SendMessageW(ctrl(hwnd, id), CB_SETCURSEL, i, 0);
}

static void combo_clear(HWND hwnd, int id) {
    SendMessageW(ctrl(hwnd, id), CB_RESETCONTENT, 0, 0);
}

/* ════════════════════════════════════════════════════════════════════
   UNIT PANEL
   ════════════════════════════════════════════════════════════════════ */

void panel_unit_populate(HWND hwnd) {
    int i;
    combo_clear(hwnd, ID_UNIT_TYPE);
    for (i = 0; i < g_unit_cat_count; i++)
        combo_add(hwnd, ID_UNIT_TYPE, g_unit_cats[i].type_name);
    combo_sel(hwnd, ID_UNIT_TYPE, 0);

    combo_clear(hwnd, ID_UNIT_FROM_UNT);
    combo_clear(hwnd, ID_UNIT_TO_UNT);
    for (i = 0; i < g_unit_cats[0].unit_count; i++) {
        combo_add(hwnd, ID_UNIT_FROM_UNT, g_unit_cats[0].units[i].name);
        combo_add(hwnd, ID_UNIT_TO_UNT,   g_unit_cats[0].units[i].name);
    }
    combo_sel(hwnd, ID_UNIT_FROM_UNT, 0);
    combo_sel(hwnd, ID_UNIT_TO_UNT,   0);
}

static void unit_type_changed(HWND hwnd) {
    int i;
    int cat = get_combo_sel(hwnd, ID_UNIT_TYPE);
    if (cat < 0) return;

    combo_clear(hwnd, ID_UNIT_FROM_UNT);
    combo_clear(hwnd, ID_UNIT_TO_UNT);
    for (i = 0; i < g_unit_cats[cat].unit_count; i++) {
        combo_add(hwnd, ID_UNIT_FROM_UNT, g_unit_cats[cat].units[i].name);
        combo_add(hwnd, ID_UNIT_TO_UNT,   g_unit_cats[cat].units[i].name);
    }
    combo_sel(hwnd, ID_UNIT_FROM_UNT, 0);
    combo_sel(hwnd, ID_UNIT_TO_UNT,   1 < g_unit_cats[cat].unit_count ? 1 : 0);
    set_ctrl_text(hwnd, ID_UNIT_FROM_VAL, L"");
    set_ctrl_text(hwnd, ID_UNIT_TO_VAL, L"");
}

void panel_unit_convert(HWND hwnd) {
    int cat   = get_combo_sel(hwnd, ID_UNIT_TYPE);
    int from  = get_combo_sel(hwnd, ID_UNIT_FROM_UNT);
    int to    = get_combo_sel(hwnd, ID_UNIT_TO_UNT);
    WCHAR buf[64]; 
    WCHAR resbuf[64];
    double val, result;

    if (cat < 0) return;

    get_ctrl_text(hwnd, ID_UNIT_FROM_VAL, buf, 64);
    val = wcstod(buf, NULL);

    if (cat == 6) { /* Temperature index */
        double c = temp_to_celsius(val, from);
        result   = celsius_to_unit(c, to);
    } else {
        double base = val * g_unit_cats[cat].units[from].to_base;
        result = base / g_unit_cats[cat].units[to].to_base;
    }

    swprintf(resbuf, 64, L"%.10g", result);
    set_ctrl_text(hwnd, ID_UNIT_TO_VAL, resbuf);
}

void panel_unit_on_command(HWND hwnd, int id) {
    if (id == ID_UNIT_TYPE) {
        unit_type_changed(hwnd);
    }
    panel_unit_convert(hwnd);
}

/* ════════════════════════════════════════════════════════════════════
   DATE PANEL
   ════════════════════════════════════════════════════════════════════ */

static void systemtime_to_str(const SYSTEMTIME *st, WCHAR *buf, int n) {
    swprintf(buf, n, L"%04d-%02d-%02d", st->wYear, st->wMonth, st->wDay);
}

static long long date_to_jdn(int y, int m, int d) {
    long long a = (14 - m) / 12;
    long long Y = y + 4800 - a;
    long long M = m + 12 * a - 3;
    return d + (153 * M + 2) / 5 + 365 * Y + Y / 4 - Y / 100 + Y / 400 - 32045;
}

void panel_date_populate(HWND hwnd) {
    SYSTEMTIME st;
    WCHAR buf[32];
    GetLocalTime(&st);
    systemtime_to_str(&st, buf, 32);
    set_ctrl_text(hwnd, ID_DATE_FROM, buf);
    set_ctrl_text(hwnd, ID_DATE_TO,   buf);

    combo_clear(hwnd, ID_DATE_TYPE);
    combo_add(hwnd, ID_DATE_TYPE, L"Calculate the difference between two dates");
    combo_add(hwnd, ID_DATE_TYPE, L"Add days to a date");
    combo_add(hwnd, ID_DATE_TYPE, L"Subtract days from a date");
    combo_sel(hwnd, ID_DATE_TYPE, 0);
}

void panel_date_calculate(HWND hwnd) {
    int mode = get_combo_sel(hwnd, ID_DATE_TYPE);
    WCHAR buf1[32], buf2[32], res1[64], res2[64];
    int y1, m1, d1, y2, m2, d2;

    get_ctrl_text(hwnd, ID_DATE_FROM, buf1, 32);
    get_ctrl_text(hwnd, ID_DATE_TO,   buf2, 32);

    if (mode == 0) {
        long long jdn1, jdn2, diff;
        if (swscanf(buf1, L"%d-%d-%d", &y1, &m1, &d1) != 3 ||
            swscanf(buf2, L"%d-%d-%d", &y2, &m2, &d2) != 3) {
            set_ctrl_text(hwnd, ID_DATE_RES1, L"Invalid date format");
            set_ctrl_text(hwnd, ID_DATE_RES2, L"");
            return;
        }
        jdn1 = date_to_jdn(y1, m1, d1);
        jdn2 = date_to_jdn(y2, m2, d2);
        diff = jdn2 - jdn1;
        if (diff < 0) diff = -diff;

        {
            int years  = (int)(diff / 365);
            int rem    = (int)(diff % 365);
            int months = rem / 30;
            int days   = rem % 30;
            int weeks  = (int)(diff / 7);
            swprintf(res1, 64, L"%d years, %d months, %d days (or %d weeks)",
                     years, months, days, weeks);
            swprintf(res2, 64, L"%lld days", diff);
        }
    } else {
        long long jdn, days;
        WCHAR *end;
        if (swscanf(buf1, L"%d-%d-%d", &y1, &m1, &d1) != 3) {
            set_ctrl_text(hwnd, ID_DATE_RES1, L"Invalid date");
            set_ctrl_text(hwnd, ID_DATE_RES2, L"");
            return;
        }
        days = wcstoll(buf2, &end, 10);
        if (mode == 2) days = -days;
        jdn = date_to_jdn(y1, m1, d1) + days;

        {
            long long l, n, i2, j, ry, rm, rd;
            l = jdn + 68569;
            n = (4 * l) / 146097;
            l = l - (146097 * n + 3) / 4;
            i2 = (4000 * (l + 1)) / 1461001;
            l = l - (1461 * i2) / 4 + 31;
            j = (80 * l) / 2447;
            rd = l - (2447 * j) / 80;
            l = j / 11;
            rm = j + 2 - 12 * l;
            ry = 100 * (n - 49) + i2 + l;
            swprintf(res1, 64, L"%04lld-%02lld-%02lld", ry, rm, rd);
            res2[0] = L'\0';
        }
    }
    set_ctrl_text(hwnd, ID_DATE_RES1, res1);
    set_ctrl_text(hwnd, ID_DATE_RES2, res2);
}

/* ════════════════════════════════════════════════════════════════════
   WORKSHEETS
   ════════════════════════════════════════════════════════════════════ */

/* All five mortgage fields.  The combo "solve for" index picks which
   one is the output; the other four become input labels/fields. */
static const WCHAR *ws_mortgage_all[] = {
    L"Monthly payment", L"Purchase price", L"Down payment",
    L"Term (years)", L"Interest rate (%)"
};
#define MORTGAGE_FIELD_COUNT 5

static const WCHAR *ws_vehicle_labels[] = {
    L"MSRP", L"Residual value (%)", L"Down payment",
    L"Lease term (months)", L"Interest rate (%)", NULL
};
static const WCHAR *ws_mpg_labels[] = {
    L"Distance (miles)", L"Fuel used (gallons)", NULL
};
static const WCHAR *ws_lkm_labels[] = {
    L"Distance (kilometers)", L"Fuel used (liters)", NULL
};

/* ── Mortgage: populate labels based on combo selection ──────────── */
static void mortgage_populate_fields(HWND hwnd) {
    static const int lbl_ids[] = {ID_WS_LBL1,ID_WS_LBL2,ID_WS_LBL3,
                                   ID_WS_LBL4,ID_WS_LBL5,ID_WS_LBL6};
    static const int inp_ids[] = {ID_WS_IN1,ID_WS_IN2,ID_WS_IN3,
                                   ID_WS_IN4,ID_WS_IN5,ID_WS_IN6};
    int solve = get_combo_sel(hwnd, ID_WS_COMBO);
    int slot = 0, i;

    if (solve < 0) solve = 0;

    /* Show the four fields that are NOT the solve-for target */
    for (i = 0; i < MORTGAGE_FIELD_COUNT; i++) {
        HWND hl, hi;
        if (i == solve) continue;
        hl = ctrl(hwnd, lbl_ids[slot]);
        hi = ctrl(hwnd, inp_ids[slot]);
        if (hl) { SetWindowTextW(hl, ws_mortgage_all[i]); ShowWindow(hl, SW_SHOW); }
        if (hi) ShowWindow(hi, SW_SHOW);
        slot++;
    }
    /* Hide unused slots */
    for (i = slot; i < 6; i++) {
        HWND hl = ctrl(hwnd, lbl_ids[i]);
        HWND hi = ctrl(hwnd, inp_ids[i]);
        if (hl) ShowWindow(hl, SW_HIDE);
        if (hi) ShowWindow(hi, SW_HIDE);
    }
    set_ctrl_text(hwnd, ID_WS_RES, L"");
}

/* ── Generic populate for non-mortgage worksheets ────────────────── */
static void generic_populate_fields(HWND hwnd, const WCHAR **labels) {
    static const int lbl_ids[] = {ID_WS_LBL1,ID_WS_LBL2,ID_WS_LBL3,
                                   ID_WS_LBL4,ID_WS_LBL5,ID_WS_LBL6};
    static const int inp_ids[] = {ID_WS_IN1,ID_WS_IN2,ID_WS_IN3,
                                   ID_WS_IN4,ID_WS_IN5,ID_WS_IN6};
    int i;
    for (i = 0; i < 6; i++) {
        HWND hl = ctrl(hwnd, lbl_ids[i]);
        HWND hi = ctrl(hwnd, inp_ids[i]);
        if (labels[i]) {
            if (hl) { SetWindowTextW(hl, labels[i]); ShowWindow(hl, SW_SHOW); }
            if (hi) { SetWindowTextW(hi, L""); ShowWindow(hi, SW_SHOW); }
        } else {
            if (hl) ShowWindow(hl, SW_HIDE);
            if (hi) ShowWindow(hi, SW_HIDE);
        }
    }
    set_ctrl_text(hwnd, ID_WS_RES, L"");
}

void panel_ws_populate(HWND hwnd, int ws_type) {
    switch (ws_type) {
        case WS_MORTGAGE:
            mortgage_populate_fields(hwnd);
            break;
        case WS_VEHICLE:
            generic_populate_fields(hwnd, ws_vehicle_labels);
            break;
        case WS_FUEL_MPG:
            generic_populate_fields(hwnd, ws_mpg_labels);
            break;
        default:
            generic_populate_fields(hwnd, ws_lkm_labels);
            break;
    }
}

/* Called when the worksheet combo selection changes */
void panel_ws_combo_changed(HWND hwnd, int ws_type) {
    if (ws_type == WS_MORTGAGE)
        mortgage_populate_fields(hwnd);
}

static double get_ws_val(HWND hwnd, int id) {
    WCHAR buf[64];
    GetWindowTextW(ctrl(hwnd, id), buf, 64);
    return wcstod(buf, NULL);
}

/* ── Mortgage solve ─────────────────────────────────────────────────
   The four input fields map to whichever mortgage variables are NOT
   the solve-for target. We read them in order and assign to the
   four "other" variables. */
static void mortgage_calculate(HWND hwnd) {
    /* Move all declarations to the top of the block */
    static const int inp_ids[] = {ID_WS_IN1, ID_WS_IN2, ID_WS_IN3, ID_WS_IN4};
    int solve;
    double vals[4];
    double price, down, years, rate, payment;
    double r, n, pn, principal;
    WCHAR res[128];
    int slot = 0, i;

    /* Executable code starts here */
    solve = get_combo_sel(hwnd, ID_WS_COMBO);
    if (solve < 0) solve = 0;

    /* Read the 4 input fields */
    for (i = 0; i < 4; i++)
        vals[i] = get_ws_val(hwnd, inp_ids[i]);

    /* Map vals[] back to the 5 mortgage variables.
       The solve-for variable gets 0; the rest get vals[slot++]. */
    slot = 0;
    payment = price = down = years = rate = 0.0;
    for (i = 0; i < MORTGAGE_FIELD_COUNT; i++) {
        double v = (i == solve) ? 0.0 : vals[slot++];
        switch (i) {
            case 0: payment = v; break;
            case 1: price   = v; break;
            case 2: down    = v; break;
            case 3: years   = v; break;
            case 4: rate    = v; break;
        }
    }

    r = rate / 100.0 / 12.0;
    n = years * 12.0;

    switch (solve) {
        case 0: /* Solve for Monthly payment */
            principal = price - down;
            if (r == 0.0)
                payment = (n > 0) ? principal / n : 0;
            else {
                pn = pow(1 + r, n);
                payment = principal * r * pn / (pn - 1.0);
            }
            swprintf(res, 128, L"Monthly payment: $%.2f", payment);
            break;

        case 1: /* Solve for Purchase price */
            if (r == 0.0)
                price = (n > 0) ? payment * n + down : down;
            else {
                pn = pow(1 + r, n);
                price = payment * (pn - 1.0) / (r * pn) + down;
            }
            swprintf(res, 128, L"Purchase price: $%.2f", price);
            break;

        case 2: /* Solve for Down payment */
            if (r == 0.0)
                principal = (n > 0) ? payment * n : 0;
            else {
                pn = pow(1 + r, n);
                principal = payment * (pn - 1.0) / (r * pn);
            }
            down = price - principal;
            swprintf(res, 128, L"Down payment: $%.2f", down);
            break;

        case 3: /* Solve for Term (years) */
            principal = price - down;
            if (r == 0.0)
                years = (payment > 0) ? principal / payment / 12.0 : 0;
            else {
                double x = 1.0 - principal * r / payment;
                if (x <= 0) { swprintf(res, 128, L"Cannot solve"); break; }
                n = -log(x) / log(1 + r);
                years = n / 12.0;
            }
            swprintf(res, 128, L"Term: %.1f years", years);
            break;

        case 4: /* Solve for Interest rate — Newton's method */
        {
            double guess = 0.05 / 12.0; 
            int iter;
            principal = price - down;
            if (principal <= 0 || payment <= 0 || n <= 0) {
                swprintf(res, 128, L"Invalid inputs"); break;
            }
            for (iter = 0; iter < 200; iter++) {
                double pg = pow(1 + guess, n);
                double f  = principal * guess * pg / (pg - 1.0) - payment;
                double h = 1e-8;
                double pg2 = pow(1 + guess + h, n);
                double f2  = principal * (guess+h) * pg2 / (pg2 - 1.0) - payment;
                double dp = (f2 - f) / h;
                if (fabs(dp) < 1e-15) break;
                guess -= f / dp;
                if (guess < 0) guess = 1e-6;
                if (fabs(f) < 0.001) break;
            }
            rate = guess * 12.0 * 100.0;
            swprintf(res, 128, L"Interest rate: %.3f%%", rate);
            break;
        }
        default:
            res[0] = L'\0';
    }
    set_ctrl_text(hwnd, ID_WS_RES, res);
}

void panel_ws_calculate(HWND hwnd, int ws_type) {
    WCHAR res[128];

    switch (ws_type) {
        case WS_MORTGAGE:
            mortgage_calculate(hwnd);
            return;
        case WS_VEHICLE: {
            double msrp     = get_ws_val(hwnd, ID_WS_IN1);
            double res_pct  = get_ws_val(hwnd, ID_WS_IN2);
            double down     = get_ws_val(hwnd, ID_WS_IN3);
            double months   = get_ws_val(hwnd, ID_WS_IN4);
            double rate     = get_ws_val(hwnd, ID_WS_IN5);
            double residual = msrp * res_pct / 100.0;
            double depreciation = (msrp - down - residual) / (months > 0 ? months : 1);
            double finance_charge = (msrp - down + residual) * (rate / 100.0 / 24.0);
            double monthly = depreciation + finance_charge;
            swprintf(res, 128, L"Monthly lease: $%.2f", monthly);
            break;
        }
        case WS_FUEL_MPG: {
            double dist = get_ws_val(hwnd, ID_WS_IN1);
            double fuel = get_ws_val(hwnd, ID_WS_IN2);
            double mpg  = (fuel > 0) ? dist / fuel : 0;
            swprintf(res, 128, L"Fuel economy: %.2f mpg", mpg);
            break;
        }
        case WS_FUEL_LKM: {
            double dist = get_ws_val(hwnd, ID_WS_IN1);
            double fuel = get_ws_val(hwnd, ID_WS_IN2);
            double lkm  = (dist > 0) ? (fuel / dist) * 100.0 : 0;
            swprintf(res, 128, L"Fuel economy: %.2f L/100 km", lkm);
            break;
        }
        default:
            res[0] = L'\0';
    }
    set_ctrl_text(hwnd, ID_WS_RES, res);
}