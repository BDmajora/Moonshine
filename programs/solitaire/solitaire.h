#ifndef __SOLITAIRE_H__
#define __SOLITAIRE_H__

/* Resource IDs */
#define IDI_SOLITAIRE      100
#define IDM_GAME_DEAL      101
#define IDM_GAME_EXIT      102
#define IDM_HELP_ABOUT     103
#define IDM_HELP_CONTENTS  104
#define IDR_MAINMENU       200

#define SOL_BG_COLOR       RGB(0, 128, 0)

/* The Resource Compiler (wrc) must skip this part */
#ifndef RC_INVOKED
#include <cards.h>

typedef struct {
    int card_id;
    BOOL face_up;
} CARD_STACK;

extern void InitGame(void);
extern void DrawBoard(HDC hdc, int width, int height);

/* Wine cards.dll exports - helps clear implicit declaration warnings */
extern BOOL WINAPI cdtInit(int *width, int *height);
extern void WINAPI cdtTerm(void);
extern BOOL WINAPI cdtDraw(HDC hdc, int x, int y, int card, int mode, COLORREF color);
#endif

#endif /* __SOLITAIRE_H__ */