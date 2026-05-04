#ifndef SOL_CARDS_H
#define SOL_CARDS_H

#include "solitaire.h"

// Card property helpers
int  Card_Suit(CARD c);
int  Card_Face(CARD c);
BOOL Card_IsRed(CARD c);

// Array manipulation
void Card_Shuffle(CARD *arr, int n);

#endif