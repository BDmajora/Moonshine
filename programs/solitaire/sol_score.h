#ifndef SOL_SCORE_H
#define SOL_SCORE_H

// Lifecycle and State
void Score_Init(void);
void Score_Reset(void);
void Score_SetCheat(void);

// Point Entry
void Score_Add(int points);      // For standard gameplay
void Score_AddBonus(int points); // For endgame animation

// Data Retrieval
int  Score_GetTotal(void);
int  Score_GetBonus(void);

#endif