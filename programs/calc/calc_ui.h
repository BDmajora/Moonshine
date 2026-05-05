#ifndef CALC_UI_H
#define CALC_UI_H

/* Facade header to maintain compatibility with legacy includes */
#include <windows.h>
#include "calc.h"

/* Global UI state */
extern int  g_mode;
extern int  g_panel;
extern int  g_worksheet;
extern BOOL g_basic_mode;

extern HFONT hBtnFont;
extern HFONT hDispFont;
extern HFONT hSmallFont;

#include "calc_widgets.h"
#include "calc_create.h"
#include "calc_layout.h"
#include "calc_update.h"

#endif /* CALC_UI_H */