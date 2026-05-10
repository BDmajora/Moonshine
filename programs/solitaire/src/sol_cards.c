#include "solitaire.h"
#include "sol_cards.h"
#include <stdlib.h>

// Returns the suit index (0-3) by masking the lowest 2 bits 
int Card_Suit(CARD c) { return c & 3; }

// Returns the card value (0-12) by shifting out the suit bits
int Card_Face(CARD c) { return c >> 2; }

// Evaluates if a card is red based on the extracted suit 
BOOL Card_IsRed(CARD c) {
    int s = Card_Suit(c); 
    return (s == CARD_SUIT_DIAMONDS || s == CARD_SUIT_HEARTS);
}

// Populates an array with a standard 52-card deck
void Card_GenerateDeck(CARD *deck) {
    int i;
    for (i = 0; i < 52; i++) {
        deck[i] = (CARD)i;
    }
}

// Fisher-Yates Shuffle: provides an unbiased, random permutation
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