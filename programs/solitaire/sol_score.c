#include "sol_score.h"
#include "sol_endgame.h"

static int g_base_score  = 0;
static int g_bonus_score = 0;
static int g_is_cheat    = 0;

void Score_Init(void) {
    g_base_score  = 0;
    g_bonus_score = 0;
    g_is_cheat    = 0;
}

// Map Reset to Init for consistency with your sol_endgame.c calls
void Score_Reset(void) {
    Score_Init();
}

void Score_Add(int points) {
    // Block standard score additions if the victory animation is running
    if (g_endgame_active) return;
    
    g_base_score += points;
    if (g_base_score < 0) g_base_score = 0;
}

void Score_AddBonus(int points) {
    if (!g_is_cheat) {
        g_bonus_score += points;
    }
}

void Score_SetCheat(void) {
    g_is_cheat    = 1;
    g_base_score  = 0; // Wipe play score
    g_bonus_score = 0; // Wipe existing bonus
}

int Score_GetTotal(void) {
    return g_base_score + g_bonus_score;
}

int Score_GetBonus(void) {
    return g_bonus_score;
}