#ifndef CALC_APPMENU_H
#define CALC_APPMENU_H

#include <windows.h>

HMENU create_menu(void);
void get_window_size(int client_w, int client_h, DWORD style, int *ww, int *wh);
int mode_client_width(void);
int mode_client_height(void);

#endif /* CALC_APPMENU_H */