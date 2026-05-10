/* ────────────────────────────────────────────────────────────────────
   calc_create.c — top-level control creation dispatcher.

   Each mode has its own create function in calc_mode*.c.  Side panels
   (unit/history/worksheet) stay inline since they're small.  Date
   panel goes through calc_dateCalculation.c.
   ──────────────────────────────────────────────────────────────────── */
#include "calc_defs.h"
#include "calc_panel.h"
#include "calc_mortgage.h"
#include "calc_vehicleLease.h"
#include "calc_dateCalculation.h"
#include "calc_modeStandard.h"
#include "calc_modeScientific.h"
#include "calc_modeProgrammer.h"
#include "calc_modeStatistics.h"

/* Wrapper: raw CreateWindowW + register in one call */
static HWND make_raw(HWND parent, LPCWSTR cls, LPCWSTR text,
                     DWORD style, int id) {
    HWND h = CreateWindowW(cls, text, style,
                           0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, NULL, NULL);
    ui_register_child(h);
    return h;
}

static void create_display(HWND hwnd) {
    make_raw(hwnd, L"STATIC", L"0",
             WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE | SS_SUNKEN,
             ID_DISPLAY);
}

/* ── Side panels (small, kept inline) ────────────────────────────── */

static void create_unit_panel(HWND hwnd) {
    make_label(hwnd, 350,            L"Select the type of unit you want to convert");
    make_combo(hwnd, ID_UNIT_TYPE);
    make_label(hwnd, 351,            L"From");
    make_edit( hwnd, ID_UNIT_FROM_VAL, L"Enter value");
    make_combo(hwnd, ID_UNIT_FROM_UNT);
    make_label(hwnd, 352,            L"To");
    make_edit( hwnd, ID_UNIT_TO_VAL,   L"");
    make_combo(hwnd, ID_UNIT_TO_UNT);
    panel_unit_populate(hwnd);
}

static void create_date_panel(HWND hwnd) {
    /* Delegated to calc_dateCalculation.c */
    date_create_controls(hwnd);
}

static void create_history_panel(HWND hwnd) {
    make_label(hwnd, 356, L"History");
    make_raw(hwnd, L"LISTBOX", NULL,
             WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
             ID_PANEL_COMBO);
    make_button(hwnd, ID_PANEL_CLOSE, L"Clear History", FALSE);
}

static void create_worksheet_panel(HWND hwnd, int ws_type) {
    int i;
    static const int lbl_ids[] = {ID_WS_LBL1,ID_WS_LBL2,ID_WS_LBL3,
                                   ID_WS_LBL4,ID_WS_LBL5,ID_WS_LBL6};
    static const int inp_ids[] = {ID_WS_IN1,ID_WS_IN2,ID_WS_IN3,
                                   ID_WS_IN4,ID_WS_IN5,ID_WS_IN6};

    make_label(hwnd, 357, L"Select the value you want to calculate");
    make_combo(hwnd, ID_WS_COMBO);

    for (i = 0; i < 6; i++) {
        make_label(hwnd, lbl_ids[i], L"");
        make_edit( hwnd, inp_ids[i], L"Enter value");
    }

    make_button(hwnd, ID_WS_CALC, L"Calculate", TRUE);
    make_label( hwnd, ID_WS_RES,  L"");
    panel_ws_populate(hwnd, ws_type);
}

/* ── Public entry point ──────────────────────────────────────────── */

void ui_create_controls(HWND hwnd) {
    create_display(hwnd);
    switch (g_mode) {
        case MODE_SCIENTIFIC: mode_sci_create(hwnd);  break;
        case MODE_PROGRAMMER: mode_prog_create(hwnd); break;
        case MODE_STATISTICS: mode_stat_create(hwnd); break;
        default:              mode_std_create(hwnd);  break;
    }
    switch (g_panel) {
        case PANEL_HISTORY:   create_history_panel(hwnd);              break;
        case PANEL_UNIT:      create_unit_panel(hwnd);                 break;
        case PANEL_DATE:      create_date_panel(hwnd);                 break;
        case PANEL_WORKSHEET: create_worksheet_panel(hwnd,g_worksheet);break;
    }
}