#ifndef UI_UPDATE_H
#define UI_UPDATE_H

#include <windows.h>

void ui_update_layout(HWND hwnd);
void ui_update_display(HWND hwnd);
void ui_update_bit_display(HWND hwnd);
void ui_update_stat_display(HWND hwnd);
void ui_update_history_panel(HWND hwnd);
void ui_update_prog_buttons(HWND hwnd);
void ui_update_digit_grouping(HWND hwnd);

void ui_show_panel(HWND hwnd, int panel);
void ui_show_worksheet(HWND hwnd, int ws_type);
void ui_rebuild_mode(HWND hwnd);
void ui_update_menu_check(HMENU hMenu);

#endif /* UI_UPDATE_H */