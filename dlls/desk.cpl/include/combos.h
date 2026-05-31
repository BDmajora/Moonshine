/* desk.cpl — main page combo population. */

#ifndef DESK_COMBOS_H
#define DESK_COMBOS_H

#include "desk_private.h"

void populate_output_combo(HWND hwnd);
void populate_resolution_combo(HWND hwnd);
void populate_orientation_combo(HWND hwnd);
void read_resolution_combo(HWND hwnd);

#endif /* DESK_COMBOS_H */