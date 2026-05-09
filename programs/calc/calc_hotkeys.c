/* ────────────────────────────────────────────────────────────────────
   calc_hotkeys.c — keyboard shortcut dispatcher.

   Three sections:
     1. Modifier helpers (Ctrl/Alt/Shift state checks)
     2. Hotkey table (vk + modifier → command id)
     3. Dispatcher functions (WM_KEYDOWN/WM_SYSKEYDOWN, WM_CHAR)

   The dispatcher returns TRUE when a key is handled, mirroring
   sol_input.c's Input_OnKeyboard signature.  WndProc skips
   DefWindowProc and IsDialogMessage in that case to prevent menus
   from intercepting Alt-combinations.
   ──────────────────────────────────────────────────────────────────── */
#include <windows.h>
#include <wchar.h>
#include "calc.h"
#include "calc_hotkeys.h"

/* Modifier flags packed into a single int for the lookup table. */
#define HK_NONE  0x00
#define HK_CTRL  0x01
#define HK_ALT   0x02
#define HK_SHIFT 0x04

static int current_mods(void) {
    int m = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) m |= HK_CTRL;
    if (GetKeyState(VK_MENU)    & 0x8000) m |= HK_ALT;
    if (GetKeyState(VK_SHIFT)   & 0x8000) m |= HK_SHIFT;
    return m;
}

/* ── Hotkey table ────────────────────────────────────────────────── */

typedef struct {
    int  vk;        /* virtual key */
    int  mods;      /* HK_* mask  */
    int  cmd;       /* command id to dispatch via on_command */
} Hotkey;

static const Hotkey s_hotkeys[] = {
    /* Mode switching — Alt+1..4 */
    { '1',        HK_ALT,  ID_VIEW_STANDARD   },
    { '2',        HK_ALT,  ID_VIEW_SCIENTIFIC },
    { '3',        HK_ALT,  ID_VIEW_PROGRAMMER },
    { '4',        HK_ALT,  ID_VIEW_STATISTICS },

    /* Panel switching */
    { 'H',        HK_CTRL, ID_VIEW_HISTORY    },
    { 'U',        HK_CTRL, ID_PANEL_UNIT      },
    { 'E',        HK_CTRL, ID_PANEL_DATE      },
    { VK_F4,      HK_CTRL, ID_VIEW_BASIC      },

    /* Memory shortcuts (standard Windows Calculator) */
    { 'L',        HK_CTRL, ID_MC              },
    { 'R',        HK_CTRL, ID_MR              },
    { 'M',        HK_CTRL, ID_MS              },
    { 'P',        HK_CTRL, ID_MPLUS           },
    { 'Q',        HK_CTRL, ID_MMINUS          },

    /* Edit */
    { 'C',        HK_CTRL, 500                },  /* Copy  */
    { 'V',        HK_CTRL, 501                },  /* Paste */

    /* Display toggles */
    { VK_F3,      HK_NONE, ID_SCI_FE          },  /* F-E toggle */
    { VK_F9,      HK_NONE, ID_SIGN            },  /* ± */

    /* Programmer base radios */
    { VK_F5,      HK_NONE, ID_RADIO_HEX       },
    { VK_F6,      HK_NONE, ID_RADIO_DEC       },
    { VK_F7,      HK_NONE, ID_RADIO_OCT       },
    { VK_F8,      HK_NONE, ID_RADIO_BIN       },

    /* Programmer word radios */
    { VK_F12,     HK_NONE, ID_RADIO_QWORD     },

    /* Help */
    { VK_F1,      HK_NONE, ID_HELP_ABOUT      },

    /* Editing — no modifiers */
    { VK_ESCAPE,  HK_NONE, ID_CLR             },
    { VK_DELETE,  HK_NONE, ID_CE              },
    { VK_BACK,    HK_NONE, ID_BACK            },
    { VK_RETURN,  HK_NONE, ID_EQ              },
};

#define HOTKEY_COUNT (sizeof(s_hotkeys) / sizeof(s_hotkeys[0]))

/* ── Key-down dispatcher ─────────────────────────────────────────── */

static BOOL handle_keydown(HWND hwnd, WPARAM vk) {
    int mods = current_mods();
    size_t i;

    /* Normalize lowercase ASCII to upper to match table entries. */
    if (vk >= 'a' && vk <= 'z') vk = (WPARAM)(vk - 'a' + 'A');

    for (i = 0; i < HOTKEY_COUNT; i++) {
        if (s_hotkeys[i].vk == (int)vk && s_hotkeys[i].mods == mods) {
            on_command(hwnd, s_hotkeys[i].cmd);
            return TRUE;
        }
    }
    return FALSE;
}

/* ── WM_CHAR dispatcher (digits, hex letters, operators) ─────────── */

static BOOL handle_char(HWND hwnd, WCHAR ch) {
    /* Ignore characters when modifier keys are held — those are
       hotkeys handled by handle_keydown.  Otherwise Ctrl+L would
       both clear memory AND insert 'l'. */
    if (current_mods() & (HK_CTRL | HK_ALT)) return FALSE;

    if (ch >= L'0' && ch <= L'9') {
        on_command(hwnd, ID_0 + (ch - L'0'));
        return TRUE;
    }
    if (ch >= L'a' && ch <= L'f') ch = (WCHAR)(ch - L'a' + L'A');
    switch (ch) {
        case L'A': on_command(hwnd, ID_A);       return TRUE;
        case L'B': on_command(hwnd, ID_B);       return TRUE;
        case L'C': on_command(hwnd, ID_C_HEX);   return TRUE;
        case L'D': on_command(hwnd, ID_D);       return TRUE;
        case L'E': on_command(hwnd, ID_E_HEX);   return TRUE;
        case L'F': on_command(hwnd, ID_F);       return TRUE;
        case L'+': on_command(hwnd, ID_ADD);     return TRUE;
        case L'-': on_command(hwnd, ID_SUB);     return TRUE;
        case L'*': on_command(hwnd, ID_MUL);     return TRUE;
        case L'/': on_command(hwnd, ID_DIV);     return TRUE;
        case L'.': on_command(hwnd, ID_DOT);     return TRUE;
        case L'=': on_command(hwnd, ID_EQ);      return TRUE;
        case L'%': on_command(hwnd, ID_PERCENT); return TRUE;
        case L'@': on_command(hwnd, ID_SQRT);    return TRUE;
    }
    return FALSE;
}

/* ── Public entry point ──────────────────────────────────────────── */

BOOL Hotkeys_OnKey(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            return handle_keydown(hwnd, wp);
        case WM_CHAR:
            return handle_char(hwnd, (WCHAR)wp);
        default:
            return FALSE;
    }
}