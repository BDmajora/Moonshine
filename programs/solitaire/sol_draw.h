#ifndef SOL_DRAW_H
#define SOL_DRAW_H

#include <windows.h>

/* Drawing Constants */
#define STATUS_MARGIN      12
#define WASTE_FAN_OFF      12
#define CARD_BACK_RED      54

/* Drag Source Types */
#define SRC_WASTE          1
#define SRC_TAB            2
#define SRC_FOUND          3

/* Core Drawing Functions */
void OnTimer(HWND hwnd);
void DrawBoard(HDC hdc, int width, int height);

#endif /* SOL_DRAW_H */