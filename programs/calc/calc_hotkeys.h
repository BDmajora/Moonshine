#ifndef CALC_HOTKEYS_H
#define CALC_HOTKEYS_H

#include <windows.h>

/* ────────────────────────────────────────────────────────────────────
   calc_hotkeys.h — keyboard shortcut dispatcher.
   ──────────────────────────────────────────────────────────────────── */

/* Build (once) and return the accelerator table for the message loop
   to pass to TranslateAcceleratorW.  This is the primary mechanism;
   it converts hotkeys into WM_COMMAND messages BEFORE the wndproc
   sees them as WM_KEYDOWN, bypassing IsDialogMessageW interception. */
HACCEL Hotkeys_GetAccelTable(void);

/* Manual fallback handler.  Returns TRUE if the message was handled.
   The wndproc still calls this for WM_KEYDOWN/WM_SYSKEYDOWN/WM_CHAR
   so character keys (digits, operators, hex letters) and editing keys
   (Esc/Del/Backspace/Enter) still work. */
BOOL Hotkeys_OnKey(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

#endif /* CALC_HOTKEYS_H */