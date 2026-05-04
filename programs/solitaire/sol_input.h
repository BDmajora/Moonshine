#ifndef SOL_INPUT_H
#define SOL_INPUT_H

#include <windows.h>

/* High-level event dispatchers for solitaire.c */
void Input_OnMouse(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
BOOL Input_OnKeyboard(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void Input_OnCommand(HWND hwnd, int menuId);
void Input_OnTimer(HWND hwnd, WPARAM timerId);

/* Internal game action handlers */
void OnLButtonDown(HWND hwnd, int mx, int my);
void OnMouseMove(HWND hwnd, int mx, int my);
void OnLButtonUp(HWND hwnd, int mx, int my);
void OnTimer(HWND hwnd);

/* Cheat / Utility logic */
void Input_CheatWin(HWND hwnd);

#endif /* SOL_INPUT_H */