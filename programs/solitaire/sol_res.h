#ifndef SOL_RES_H
#define SOL_RES_H

#include <windows.h>

// Window class identifier
#define SOL_WC_NAME L"SolitaireWnd"

// Window dimension constraints
#define MIN_WIDTH   560
#define MIN_HEIGHT  400
#define DEFAULT_TAB_BOTTOM 384

// Creates and initializes the main game window
HWND Res_InitWindow(HINSTANCE hInst, int nCmdShow, WNDPROC lpfnWndProc);

// Sets window scaling limits
void Res_GetMinMaxInfo(LPARAM lp);

#endif // SOL_RES_H