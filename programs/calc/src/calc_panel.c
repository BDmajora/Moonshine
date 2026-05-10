#include "calc_panel.h"
#include "calc_logic.h"
#include "calc_ui.h"
#include "calc_mortgage.h"
#include "calc_vehicleLease.h"
#include "calc_fuelEconomy.h"
#include "calc_dateCalculation.h"
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

   Delegated to calc_dateCalculation.c — that module owns the controls
   and the math. These wrappers exist only so calc_cmdDispatch.c and
   any other external caller don't need to change.
   ════════════════════════════════════════════════════════════════════ */

void panel_date_populate(HWND hwnd) {
    /* No-op — date_create_controls (called from create_date_panel)
       sets everything up including the combo entries. */
    (void)hwnd;
}

void panel_date_calculate(HWND hwnd) {
    date_calculate(hwnd);
}


/* ════════════════════════════════════════════════════════════════════
   WORKSHEETS

   All four worksheet types now route to dedicated modules:
     WS_MORTGAGE  → calc_mortgage.c
     WS_VEHICLE   → calc_vehicleLease.c
     WS_FUEL_MPG  → calc_fuelEconomy.c (is_lkm=FALSE)
     WS_FUEL_LKM  → calc_fuelEconomy.c (is_lkm=TRUE)
   ════════════════════════════════════════════════════════════════════ */

void panel_ws_populate(HWND hwnd, int ws_type) {
    switch (ws_type) {
        case WS_MORTGAGE:  mortgage_init_combo(hwnd);          break;
        case WS_VEHICLE:   vlease_init_combo(hwnd);            break;
        case WS_FUEL_MPG:  fuel_init_combo(hwnd, FALSE);       break;
        case WS_FUEL_LKM:  fuel_init_combo(hwnd, TRUE);        break;
    }
}

/* Called from cmd dispatch when the worksheet combo selection changes. */
void panel_ws_combo_changed(HWND hwnd, int ws_type) {
    switch (ws_type) {
        case WS_MORTGAGE:  mortgage_refresh_fields(hwnd);      break;
        case WS_VEHICLE:   vlease_refresh_fields(hwnd);        break;
        case WS_FUEL_MPG:  fuel_refresh_fields(hwnd, FALSE);   break;
        case WS_FUEL_LKM:  fuel_refresh_fields(hwnd, TRUE);    break;
    }
}

void panel_ws_calculate(HWND hwnd, int ws_type) {
    switch (ws_type) {
        case WS_MORTGAGE:  mortgage_calculate(hwnd);           break;
        case WS_VEHICLE:   vlease_calculate(hwnd);             break;
        case WS_FUEL_MPG:  fuel_calculate(hwnd, FALSE);        break;
        case WS_FUEL_LKM:  fuel_calculate(hwnd, TRUE);         break;
    }
}