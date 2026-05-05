#ifndef CALC_WNDPROC_H
#define CALC_WNDPROC_H

#include <windows.h>

LRESULT CALLBACK CalcWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPWSTR cmdline, int cmdshow);

#endif /* CALC_WNDPROC_H */