#ifndef CALC_PANEL_H
#define CALC_PANEL_H

#include <windows.h>
#include "calc.h"

/* ── Unit conversion ────────────────────────────────────────────── */
typedef struct {
    const WCHAR *name;
    double to_base;   /* multiply to convert to base unit */
} UnitEntry;

typedef struct {
    const WCHAR     *type_name;
    int              unit_count;
    const UnitEntry *units;
} UnitCategory;

/* Marked extern so the linker finds these in calc_panel.c */
extern const UnitCategory g_unit_cats[];
extern const int          g_unit_cat_count;

void panel_unit_populate(HWND hwnd);
void panel_unit_convert(HWND hwnd);
void panel_unit_on_command(HWND hwnd, int id);

/* ── Date calculation ───────────────────────────────────────────── */
void panel_date_populate(HWND hwnd);
void panel_date_calculate(HWND hwnd);

/* ── Worksheets ─────────────────────────────────────────────────── */
void panel_ws_populate(HWND hwnd, int ws_type);
void panel_ws_calculate(HWND hwnd, int ws_type);

/* Called from calc_cmdDispatch when ID_WS_COMBO selection changes.
   Refreshes the input field labels to match the new solve-for target. */
void panel_ws_combo_changed(HWND hwnd, int ws_type);

#endif /* CALC_PANEL_H */