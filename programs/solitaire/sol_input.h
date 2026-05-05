#ifndef SOL_INPUT_H
#define SOL_INPUT_H

#include <windows.h>
#include "solitaire.h" /* Needed for CARD typedef */

/* Drag Source Types */
#define SRC_WASTE          1
#define SRC_TAB            2
#define SRC_FOUND          3

/* Encapsulated Drag State */
typedef struct {
    BOOL   is_dragging;
    CARD   cards[52];
    int    count;
    int    from_type;
    int    from_idx;
    int    mouse_x, mouse_y;
    int    x_off, y_off;
} DragState;

/* Accessor for drawing logic (Read-only reference) */
const DragState* Input_GetDragInfo(void);

/* High-level event dispatchers for solitaire.c */
void Input_OnMouse(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
BOOL Input_OnKeyboard(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void Input_OnCommand(HWND hwnd, int menuId);
void Input_OnTimer(HWND hwnd, WPARAM timerId);

/* External game action handlers */
void OnTimer(HWND hwnd);

/* Cheat / Utility logic */
void Input_CheatWin(HWND hwnd);

#endif /* SOL_INPUT_H */