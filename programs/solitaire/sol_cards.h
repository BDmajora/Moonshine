/* sol_cards.h */
#ifndef SOL_CARDS_H
#define SOL_CARDS_H

#include "solitaire.h"

int  Card_Suit(CARD c);
int  Card_Face(CARD c);
BOOL Card_IsRed(CARD c);
void Card_Shuffle(CARD *arr, int n);

#endif