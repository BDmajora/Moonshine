#ifndef SOL_DRAW_H
#define SOL_DRAW_H

#include <windows.h>

// UI and Layout constants
#define STATUS_MARGIN      12
#define WASTE_FAN_OFF      12
#define CARD_BACK_RED      54

// cards.dll draw modes
#define MODE_DECKX         6  // Empty stock (no reshuffle)
#define MODE_DECKO         7  // Empty stock (reshuffle available)

// Main rendering entry point
void DrawBoard(HDC hdc, int width, int height);

#endif // SOL_DRAW_H