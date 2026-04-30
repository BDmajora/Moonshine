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
#define STATUS_BAR_HEIGHT  18

/* 2. C-Only Definitions (Hidden from RC) */
#ifndef RC_INVOKED
#include <windows.h>
#include <stdlib.h>
#include <cards.h>

typedef int CARD;
#define CARD_FACEUP  0x100

/* Sub-headers */
#include "sol_cards.h"
#include "sol_game.h"
#include "sol_layout.h"

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