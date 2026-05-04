#ifndef SOL_DRAW_H
#define SOL_DRAW_H

#include <windows.h>

/* Drawing Constants */
#define STATUS_MARGIN      12
#define WASTE_FAN_OFF      12
#define CARD_BACK_RED      54

/* Standard cards.dll modes for the special empty deck markers */
#define MODE_DECKX         6  /* Red X - no cards left to deal */
#define MODE_DECKO         7  /* Green Circle - click to reshuffle waste into stock */

/* Core Drawing Functions */
void DrawBoard(HDC hdc, int width, int height);

#endif /* SOL_DRAW_H */