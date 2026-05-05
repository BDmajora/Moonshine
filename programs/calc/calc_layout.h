#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include <windows.h>
#include "calc_widgets.h"

void layout_standard(HWND hwnd, Layout *L);
void layout_scientific(HWND hwnd, Layout *L);
void layout_programmer(HWND hwnd, Layout *L);
void layout_statistics(HWND hwnd, Layout *L);
void layout_panel(HWND hwnd, Layout *L);

#endif /* UI_LAYOUT_H */