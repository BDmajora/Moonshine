/*
 * winedrm.drv — keyboard input
 *
 * Per DESIGN.md §3.3 the keyboard translation lives in the driver: glacier
 * ships xkb keysyms (already layout-resolved), and we map keysym → Win32 VK and
 * inject hardware input.  Wine's active layout then produces WM_KEYDOWN/WM_CHAR.
 * Keysyms with no VK but a printable character (symbols, accented letters) are
 * injected via KEYEVENTF_UNICODE so WM_CHAR is still correct.  Dead-key/IME
 * composition and a driver-provided KBDTABLES are later work (SPEC §9).
 *
 * Copyright 2026 the CrystallineLattice / YetiOS project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "lattice.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedrm);

/* xkb keysyms == X11 keysyms.  Defined locally so the driver keeps no xkbcommon
 * build dependency (the Makefile links none for the keysym→VK path). */
#define KS_BackSpace  0xff08
#define KS_Tab        0xff09
#define KS_Return     0xff0d
#define KS_Escape     0xff1b
#define KS_Home       0xff50
#define KS_Left       0xff51
#define KS_Up         0xff52
#define KS_Right      0xff53
#define KS_Down       0xff54
#define KS_Prior      0xff55  /* Page Up   */
#define KS_Next       0xff56  /* Page Down */
#define KS_End        0xff57
#define KS_Insert     0xff63
#define KS_Delete     0xffff
#define KS_F1         0xffbe  /* …F12 = 0xffc9 */
#define KS_Shift_L    0xffe1
#define KS_Shift_R    0xffe2
#define KS_Control_L  0xffe3
#define KS_Control_R  0xffe4
#define KS_Caps_Lock  0xffe5
#define KS_Alt_L      0xffe9
#define KS_Alt_R      0xffea
#define KS_Super_L    0xffeb
#define KS_Super_R    0xffec
#define KS_UNICODE_BASE 0x01000000u

/* Map an xkb keysym to a Win32 virtual-key code, or 0 if none applies (the
 * caller then tries the Unicode-character path). */
static UINT keysym_to_vk(uint32_t ks)
{
    if (ks >= 'a' && ks <= 'z') return (UINT)(ks - 0x20); /* -> VK_A..VK_Z */
    if (ks >= 'A' && ks <= 'Z') return (UINT)ks;
    if (ks >= '0' && ks <= '9') return (UINT)ks;          /* VK_0..VK_9    */
    if (ks >= KS_F1 && ks <= KS_F1 + 11) return VK_F1 + (ks - KS_F1);

    switch (ks)
    {
    case ' ':           return VK_SPACE;
    case KS_BackSpace:  return VK_BACK;
    case KS_Tab:        return VK_TAB;
    case KS_Return:     return VK_RETURN;
    case KS_Escape:     return VK_ESCAPE;
    case KS_Home:       return VK_HOME;
    case KS_Left:       return VK_LEFT;
    case KS_Up:         return VK_UP;
    case KS_Right:      return VK_RIGHT;
    case KS_Down:       return VK_DOWN;
    case KS_Prior:      return VK_PRIOR;
    case KS_Next:       return VK_NEXT;
    case KS_End:        return VK_END;
    case KS_Insert:     return VK_INSERT;
    case KS_Delete:     return VK_DELETE;
    case KS_Shift_L:    return VK_LSHIFT;
    case KS_Shift_R:    return VK_RSHIFT;
    case KS_Control_L:  return VK_LCONTROL;
    case KS_Control_R:  return VK_RCONTROL;
    case KS_Caps_Lock:  return VK_CAPITAL;
    case KS_Alt_L:      return VK_LMENU;
    case KS_Alt_R:      return VK_RMENU;
    case KS_Super_L:    return VK_LWIN;
    case KS_Super_R:    return VK_RWIN;
    default:            return 0;
    }
}

/* Printable character for a keysym with no VK (symbols, Latin-1, Unicode). */
static WCHAR keysym_to_char(uint32_t ks)
{
    if (ks >= 0x20 && ks <= 0x7e) return (WCHAR)ks;          /* ASCII     */
    if (ks >= 0xa0 && ks <= 0xff) return (WCHAR)ks;          /* Latin-1   */
    if ((ks & 0xff000000u) == KS_UNICODE_BASE) return (WCHAR)(ks & 0x00ffffffu);
    return 0;
}

void winedrm_key(HWND hwnd, uint32_t keysym, BOOL pressed)
{
    INPUT input = {0};
    UINT vk = keysym_to_vk(keysym);

    input.type = INPUT_KEYBOARD;

    if (vk)
    {
        HKL hkl = NtUserGetKeyboardLayout(0);
        UINT scan = NtUserMapVirtualKeyEx(vk, MAPVK_VK_TO_VSC_EX, hkl);
        input.ki.wVk = vk;
        input.ki.wScan = scan & 0xff;
        input.ki.dwFlags = pressed ? 0 : KEYEVENTF_KEYUP;
        if (scan & 0xff00) input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    else
    {
        WCHAR ch = keysym_to_char(keysym);
        if (!ch) { TRACE("hwnd=%p unmapped keysym %#x\n", hwnd, keysym); return; }
        input.ki.wScan = ch;
        input.ki.dwFlags = KEYEVENTF_UNICODE | (pressed ? 0 : KEYEVENTF_KEYUP);
    }

    TRACE("hwnd=%p keysym=%#x vk=%#x pressed=%d\n", hwnd, keysym, vk, pressed);
    NtUserSendHardwareInput(hwnd, 0, &input, 0);
}
