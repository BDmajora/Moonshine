#ifndef SOL_LAYOUT_H
#define SOL_LAYOUT_H

#define X_MARGIN        10
#define Y_MARGIN        7
#define X_SPACING       79
#define Y_TABLEAU       109
#define FACE_DOWN_OFF   3
#define FACE_UP_OFF     19

int  Layout_GetTabX(int col);
int  Layout_GetTabCardY(int col, int row);
int  Layout_HitTabCard(int col, int mx, int my);
BOOL Layout_CheckHit(int mx, int my, int cx, int cy);

#endif