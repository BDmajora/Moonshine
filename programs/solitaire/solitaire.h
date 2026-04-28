#ifndef __SOLITAIRE_H__
#define __SOLITAIRE_H__

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
#endif

#endif