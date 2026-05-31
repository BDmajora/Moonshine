/* desk.cpl — Identify overlay (flashes a number on each display). */

#ifndef DESK_IDENTIFY_H
#define DESK_IDENTIFY_H

#include "desk_private.h"

LRESULT CALLBACK identify_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
void show_identify(void);

#endif /* DESK_IDENTIFY_H */