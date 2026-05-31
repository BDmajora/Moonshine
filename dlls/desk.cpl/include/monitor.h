/* desk.cpl — Monitor tab (refresh rate, hide-modes, VRR, colors). */

#ifndef DESK_MONITOR_H
#define DESK_MONITOR_H

#include "desk_private.h"

INT_PTR CALLBACK monitor_dialog_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

#endif /* DESK_MONITOR_H */