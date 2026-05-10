#include "sol_score.h"
#include "sol_endgame.h"

static int g_base_score  = 0;
static int g_bonus_score = 0;
static int g_is_cheat    = 0;

// Resets all score values to zero
void Score_Init(void) {
    g_base_score  = 0;
    g_bonus_score = 0;
    g_is_cheat    = 0;
}

// Alias for initialization
void Score_Reset(void) {
    Score_Init();
}

// Increments base score if game is active
void Score_Add(int points) {
    if (g_endgame_active) return;
    
    g_base_score += points;
    if (g_base_score < 0) g_base_score = 0;
}

// Increments bonus score for non-cheat games
void Score_AddBonus(int points) {
    if (!g_is_cheat) {
        g_bonus_score += points;
    }
}

// Flags game as cheated and clears scores
void Score_SetCheat(void) {
    g_is_cheat    = 1;
    g_base_score  = 0;
    g_bonus_score = 0;
}

// Returns sum of base and bonus points
int Score_GetTotal(void) {
    return g_base_score + g_bonus_score;
}

// Returns current bonus points
int Score_GetBonus(void) {
    return g_bonus_score;
}