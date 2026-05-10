#ifndef CALC_APPMENU_H
#define CALC_APPMENU_H

#include <windows.h>

HMENU create_menu(void);
void get_window_size(int client_w, int client_h, DWORD style, int *ww, int *wh);

/* New sizing logic replaces mode_client_width/height */
void get_required_client_size(int mode, int panel, int *w, int *h);
void apply_window_size(HWND hwnd, int mode, int panel);

#endif /* CALC_APPMENU_H */