#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include <windows.h>

/* Layout constants computed once per resize */
typedef struct {
    int margin, gap;
    int win_w, win_h;
    int disp_x, disp_y, disp_w, disp_h;
    int grid_x, grid_y, grid_w, grid_h;
    int bw, bh;
    int rows;
    int cols;
    int panel_x, panel_w;
} Layout;

/* Control creation helpers */
HWND make_button(HWND parent, int id, const WCHAR *label, BOOL def_btn);
HWND make_radio(HWND parent, int id, const WCHAR *label, BOOL first_in_group);
HWND make_label(HWND parent, int id, const WCHAR *text);
HWND make_edit(HWND parent, int id, const WCHAR *placeholder);
HWND make_combo(HWND parent, int id);
void move_ctrl(HWND hwnd, int id, int x, int y, int w, int h);

/* Font and layout compute */
void recreate_fonts(int btn_h, int disp_h);
void compute_layout(HWND hwnd, Layout *L);

#endif /* UI_WIDGETS_H */