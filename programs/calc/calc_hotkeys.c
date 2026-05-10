/* ────────────────────────────────────────────────────────────────────
   calc_hotkeys.c — keyboard shortcut dispatcher.

   Two complementary mechanisms:

   1. Accelerator table (HACCEL) — the canonical Windows way.
      TranslateAcceleratorW in the message loop converts F-keys and
      Ctrl/Alt combinations directly into WM_COMMAND messages with
      the correct id, bypassing IsDialogMessageW interception.  This
      is the primary mechanism.

   2. Manual WM_KEYDOWN/WM_CHAR fallback — for character keys
      (digits, operators, hex letters) and editing keys (Esc/Del/
      Backspace/Enter) that don't translate through accelerators.

   ── Note on Ctrl+F4 ──────────────────────────────────────────────
   On Linux/Wine, KWin and many other window managers reserve Ctrl+F4
   system-wide (workspace/tab navigation), so the keypress never
   reaches our process.  Both Ctrl+F4 (Windows native) and Ctrl+B
   (Linux-friendly alias) are bound to ID_VIEW_BASIC; whichever the
   environment lets through will work.
   ──────────────────────────────────────────────────────────────────── */
#include <windows.h>
#include <wchar.h>
#include "calc.h"
#include "calc_hotkeys.h"

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
    int  vk;
    int  mods;
    int  cmd;
} Hotkey;

static const Hotkey s_hotkeys[] = {
    /* Mode switching */
    { '1',        HK_ALT,  ID_VIEW_STANDARD   },
    { '2',        HK_ALT,  ID_VIEW_SCIENTIFIC },
    { '3',        HK_ALT,  ID_VIEW_PROGRAMMER },
    { '4',        HK_ALT,  ID_VIEW_STATISTICS },

    /* Panel switching */
    { 'H',        HK_CTRL, ID_VIEW_HISTORY    },
    { 'U',        HK_CTRL, ID_PANEL_UNIT      },
    { 'E',        HK_CTRL, ID_PANEL_DATE      },

    /* Basic — DUAL BINDING: Ctrl+F4 (Windows) and Ctrl+B (works
       everywhere, since Ctrl+F4 is grabbed by KWin / GNOME / etc.) */
    { VK_F4,      HK_CTRL, ID_VIEW_BASIC      },
    { 'B',        HK_CTRL, ID_VIEW_BASIC      },

    /* Memory */
    { 'L',        HK_CTRL, ID_MC              },
    { 'R',        HK_CTRL, ID_MR              },
    { 'M',        HK_CTRL, ID_MS              },
    { 'P',        HK_CTRL, ID_MPLUS           },
    { 'Q',        HK_CTRL, ID_MMINUS          },

    /* Edit */
    { 'C',        HK_CTRL, 500                },
    { 'V',        HK_CTRL, 501                },

    /* Display toggles */
    { VK_F3,      HK_NONE, ID_SCI_FE          },
    { VK_F9,      HK_NONE, ID_SIGN            },

    /* Programmer base */
    { VK_F5,      HK_NONE, ID_RADIO_HEX       },
    { VK_F6,      HK_NONE, ID_RADIO_DEC       },
    { VK_F7,      HK_NONE, ID_RADIO_OCT       },
    { VK_F8,      HK_NONE, ID_RADIO_BIN       },
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

/* ── Accelerator table ───────────────────────────────────────────── */

static HACCEL s_haccel = NULL;

HACCEL Hotkeys_GetAccelTable(void) {
    ACCEL accels[HOTKEY_COUNT];
    size_t i;
    int n = 0;

    if (s_haccel) return s_haccel;

    for (i = 0; i < HOTKEY_COUNT; i++) {
        BYTE flags = FVIRTKEY;
        if (s_hotkeys[i].mods & HK_CTRL)  flags |= FCONTROL;
        if (s_hotkeys[i].mods & HK_ALT)   flags |= FALT;
        if (s_hotkeys[i].mods & HK_SHIFT) flags |= FSHIFT;
        accels[n].fVirt = flags;
        accels[n].key   = (WORD)s_hotkeys[i].vk;
        accels[n].cmd   = (WORD)s_hotkeys[i].cmd;
        n++;
    }
    s_haccel = CreateAcceleratorTableW(accels, n);
    return s_haccel;
}

/* ── Manual fallback (WM_KEYDOWN/WM_SYSKEYDOWN) ─────────────────── */

static BOOL handle_keydown(HWND hwnd, WPARAM vk) {
    int mods = current_mods();
    size_t i;

    if (vk >= 'a' && vk <= 'z') vk = (WPARAM)(vk - 'a' + 'A');

    for (i = 0; i < HOTKEY_COUNT; i++) {
        if (s_hotkeys[i].vk == (int)vk && s_hotkeys[i].mods == mods) {
            on_command(hwnd, s_hotkeys[i].cmd);
            return TRUE;
        }
    }
    return FALSE;
}

/* ── WM_CHAR (digits, operators, hex letters) ────────────────────── */

static BOOL handle_char(HWND hwnd, WCHAR ch) {
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