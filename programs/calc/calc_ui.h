#ifndef CALC_UI_H
#define CALC_UI_H

#include <windows.h>
#include "calc.h"

/* Current mode and panel state */
extern int  g_mode;
extern int  g_panel;
extern int  g_worksheet;
extern BOOL g_basic_mode;   /* hides memory row for "Basic" view */

/* Font handles (recreated on resize) */
extern HFONT hBtnFont;
extern HFONT hDispFont;
extern HFONT hSmallFont;

/* ── Functions ───────────────────────────────────────────────────── */
void ui_create_controls(HWND hwnd);
void ui_update_layout(HWND hwnd);
void ui_update_display(HWND hwnd);
void ui_update_bit_display(HWND hwnd);
void ui_update_stat_display(HWND hwnd);
void ui_rebuild_mode(HWND hwnd);   /* destroy & recreate all controls */
void ui_show_panel(HWND hwnd, int panel);
void ui_show_worksheet(HWND hwnd, int ws_type);
void ui_update_prog_buttons(HWND hwnd);  /* grey-out unavailable digits */
void ui_update_menu_check(HMENU hMenu);
void ui_update_history_panel(HWND hwnd);
void ui_update_digit_grouping(HWND hwnd);

/* Helpers */
HWND  make_button(HWND parent, int id, const WCHAR *label, BOOL def_btn);
HWND  make_radio(HWND parent, int id, const WCHAR *label, BOOL first_in_group);
HWND  make_label(HWND parent, int id, const WCHAR *text);
HWND  make_edit(HWND parent, int id, const WCHAR *placeholder);
HWND  make_combo(HWND parent, int id);
void  move_ctrl(HWND hwnd, int id, int x, int y, int w, int h);

/* Layout constants computed once per resize */
typedef struct {
    int margin, gap;
    int win_w, win_h;
    int disp_x, disp_y, disp_w, disp_h;
    int grid_x, grid_y, grid_w, grid_h;
    int bw, bh;               /* button width/height */
    int rows;                 /* number of button rows */
    int cols;                 /* number of button columns */
    int panel_x, panel_w;    /* side panel geometry */
} Layout;

void compute_layout(HWND hwnd, Layout *L);

#endif /* CALC_UI_H */