#ifndef SOL_RES_H
#define SOL_RES_H

#include <windows.h>

/* Window Class Name */
#define SOL_WC_NAME L"SolitaireWnd"

/* Resolution Constants */
#define MIN_WIDTH   560
#define MIN_HEIGHT  400
#define DEFAULT_TAB_BOTTOM 384

/* High-level window setup */
HWND Res_InitWindow(HINSTANCE hInst, int nCmdShow, WNDPROC lpfnWndProc);

/* Layout helper for WM_GETMINMAXINFO */
void Res_GetMinMaxInfo(LPARAM lp);

#endif /* SOL_RES_H */