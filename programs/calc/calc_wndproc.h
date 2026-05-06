#ifndef CALC_WNDPROC_H
#define CALC_WNDPROC_H

#include <windows.h>

/* ── Window Procedure ────────────────────────────────────────────── */
/**
 * The main message handler for the calculator window.
 * Handles resizing, button clicks, and keyboard input.
 */
LRESULT CALLBACK CalcWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

/* ── Command Dispatching ─────────────────────────────────────────── */
/**
 * Global command handler used to process button IDs and menu items.
 * This is the "glue" between your UI and the logic.
 * (Defined in calc_cmdDispatch.c)
 */
void on_command(HWND hwnd, int id);

/* ── Entry Point ─────────────────────────────────────────────────── */
/**
 * Note: While usually not necessary to prototype wWinMain in a header 
 * (as the CRT calls it directly), keeping it here is fine for 
 * documentation/completeness in small projects.
 */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPWSTR cmdline, int cmdshow);

#endif /* CALC_WNDPROC_H */