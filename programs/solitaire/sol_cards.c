#include "solitaire.h"

int Card_Suit(CARD c) { return c & 3; }
int Card_Face(CARD c) { return c >> 2; }

BOOL Card_IsRed(CARD c) {
    int s = Card_Suit(c);
    return (s == CARD_SUIT_DIAMONDS || s == CARD_SUIT_HEARTS);
}

void Card_Shuffle(CARD *arr, int n) {
    int i, j;
    CARD t;
    for (i = n - 1; i > 0; i--) {
        j = rand() % (i + 1);
        t = arr[i];
        arr[i] = arr[j];
        arr[j] = t;
    }
}