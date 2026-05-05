#ifndef SOLITAIRE_H
#define SOLITAIRE_H

// Resource and Menu identifiers
#define IDI_SOLITAIRE      100
#define IDR_MAINMENU       200
#define IDM_GAME_DEAL      101
#define IDM_GAME_EXIT      102
#define IDM_GAME_UNDO      105
#define IDM_GAME_OPTIONS   106
#define IDM_GAME_BACKS     107
#define IDM_HELP_CONTENTS  104
#define IDM_HELP_ABOUT     103

// About dialog identifiers
#define IDD_ABOUT          300
#define IDC_ABOUT_AUTHORS  301
#define IDC_ABOUT_LICENSE  302

#ifndef IDC_STATIC
#define IDC_STATIC         -1
#endif

// Visual constants
#define SOL_BG_COLOR       RGB(0,128,0)
#define STATUS_BAR_HEIGHT  17

#ifndef RC_INVOKED
#include <windows.h>
#include <stdlib.h>
#include <cards.h>

// Game state and card definitions
typedef int CARD;
#define CARD_FACEUP        0x100
#define STATUS_MARGIN      12
#define WASTE_FAN_OFF      12
#define CARD_BACK_RED      54

// Drag source constants
#define SRC_WASTE          1
#define SRC_TAB            2
#define SRC_FOUND          3

#include "sol_cards.h"
#include "sol_game.h"
#include "sol_layout.h"

// External state declarations
extern BOOL  dragging;
extern CARD  drag_cards[52];
extern int   drag_count;
extern int   drag_from_type;
extern int   drag_from_idx;
extern int   drag_mouse_x, drag_mouse_y;
extern int   drag_x_off, drag_y_off;

// Rendering and DLL interfaces
void DrawBoard(HDC hdc, int width, int height); 
extern BOOL WINAPI cdtInit(int *width, int *height);
extern void WINAPI cdtTerm(void);
extern BOOL WINAPI cdtDraw(HDC hdc, int x, int y, int card, int mode, COLORREF color);

// Victory animation interfaces
#define ENDGAME_TIMER_ID   2
extern BOOL g_endgame_active;
void EndGame_Start(HWND hwnd);
void EndGame_Stop(HWND hwnd);
void EndGame_Tick(HWND hwnd);
void EndGame_Draw(HDC hdc, int width, int height);

#endif // RC_INVOKED
#endif // SOLITAIRE_H