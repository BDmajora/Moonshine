/* desk.cpl — monitor-layout preview view. */

#ifndef DESK_PREVIEW_H
#define DESK_PREVIEW_H

#include "desk_private.h"

LRESULT CALLBACK desktop_view_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
void create_desktop_view(HWND hwnd);

#endif /* DESK_PREVIEW_H */