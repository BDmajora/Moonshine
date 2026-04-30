#ifndef SOLITAIRE_H
#define SOLITAIRE_H

/* 1. Resource IDs (Visible to both C and RC) */
#define IDI_SOLITAIRE      100
#define IDM_GAME_DEAL      101
#define IDM_GAME_EXIT      102
#define IDM_HELP_ABOUT     103
#define IDM_HELP_CONTENTS  104
#define IDR_MAINMENU       200

#define SOL_BG_COLOR       RGB(0,128,0)
#define STATUS_BAR_HEIGHT  17

/* 2. C-Only Definitions (Hidden from RC) */
#ifndef RC_INVOKED
#include <windows.h>
#include <stdlib.h>
#include <cards.h>

typedef int CARD;
#define CARD_FACEUP  0x100

/* Constants for Layout and Logic */
#define STATUS_MARGIN      12
#define WASTE_FAN_OFF      12
#define CARD_BACK_RED      54

/* Drag Source Types */
#define SRC_WASTE          1
#define SRC_TAB            2
#define SRC_FOUND          3

/* Sub-headers */
#include "sol_cards.h"
#include "sol_game.h"
#include "sol_layout.h"

/* 
 * Global Shared Drag State 
 * Defined in sol_ui.c without 'extern' or 'static'
 */
extern BOOL  dragging;
extern CARD  drag_cards[52];
extern int   drag_count;
extern int   drag_from_type;
extern int   drag_from_idx;
extern int   drag_mouse_x, drag_mouse_y;
extern int   drag_x_off, drag_y_off;

/* Global Controller Prototypes */
void DrawBoard(HDC hdc, int width, int height);
void OnLButtonDown(HWND hwnd, int mx, int my);
void OnMouseMove(HWND hwnd, int mx, int my);
void OnLButtonUp(HWND hwnd, int mx, int my);
void OnTimer(HWND hwnd);

/* cards.dll exports */
extern BOOL WINAPI cdtInit(int *width, int *height);
extern void WINAPI cdtTerm(void);
extern BOOL WINAPI cdtDraw(HDC hdc, int x, int y, int card, int mode, COLORREF color);

#endif /* RC_INVOKED */
#endif /* SOLITAIRE_H */