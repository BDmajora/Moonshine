#ifndef CALC_HOTKEYS_H
#define CALC_HOTKEYS_H

#include <windows.h>

/* ────────────────────────────────────────────────────────────────────
   calc_hotkeys.h — keyboard shortcut dispatcher.

   Modeled after solitaire's sol_input.c approach: the WndProc passes
   WM_KEYDOWN / WM_SYSKEYDOWN / WM_CHAR through this module, which
   returns TRUE if it handled the message (so WndProc can skip
   DefWindowProc and IsDialogMessage).

   Owns the table of (modifier, key, command-id) tuples so adding a
   new shortcut is a one-line change.

   Calculator shortcuts handled here:

     ── Mode switching ──
     Alt+1   Standard
     Alt+2   Scientific
     Alt+3   Programmer
     Alt+4   Statistics

     ── Panel switching ──
     Ctrl+H  History
     Ctrl+U  Unit conversion
     Ctrl+E  Date calculation
     Ctrl+F4 Basic mode

     ── Memory (standard Windows Calculator) ──
     Ctrl+L  Memory clear (MC)
     Ctrl+R  Memory recall (MR)
     Ctrl+M  Memory store  (MS)
     Ctrl+P  Memory plus   (M+)
     Ctrl+Q  Memory minus  (M-)

     ── Edit ──
     Ctrl+C  Copy
     Ctrl+V  Paste

     ── Display ──
     F3      Toggle F-E (scientific notation)
     F9      Toggle sign (±)

     ── Programmer base / word ──
     F5      Hex
     F6      Dec
     F7      Oct
     F8      Bin
     F12     Qword
     Ctrl+F2 Dword
     Ctrl+F3 Word
     Ctrl+F4 Byte (overrides Basic when in Programmer mode? — Basic wins)

     ── Editing keys ──
     Esc     Clear (C)
     Del     Clear entry (CE)
     Backspace  Backspace
     Enter   Equals
   ──────────────────────────────────────────────────────────────────── */

/* Returns TRUE if the message was handled.  Pass WM_KEYDOWN,
   WM_SYSKEYDOWN, or WM_CHAR. */
BOOL Hotkeys_OnKey(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

#endif /* CALC_HOTKEYS_H */