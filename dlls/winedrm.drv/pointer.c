/*
 * winedrm.drv — pointer input
 *
 * Injects CL_INPUT pointer events from glacier into Win32 as hardware input.
 * glacier owns the cursor (KMS plane) and sends window-local content coords;
 * we add the window's screen origin and inject absolute motion.  The cursor is
 * never drawn or set here — SetCursor is a no-op by design (SPEC §11).
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

/* evdev button codes (from <linux/input-event-codes.h>), defined locally so we
 * don't pull in <linux/input.h> — its SW_MAX collides with Win32's SW_MAX. */
#define BTN_LEFT    0x110
#define BTN_RIGHT   0x111
#define BTN_MIDDLE  0x112
#define BTN_SIDE    0x113
#define BTN_EXTRA   0x114
#define BTN_FORWARD 0x115
#define BTN_BACK    0x116

void winedrm_motion(HWND hwnd, int local_x, int local_y)
{
    struct drm_win_data *data;
    INPUT input = {0};
    POINT screen;

    if (!(data = win_data_get(hwnd))) return;
    /* glacier sends coords relative to the window's content origin; the Win32
     * window believes itself at rects.window, so screen = local + that origin.
     * Wine's driver path treats absolute mouse coords as raw screen pixels. */
    screen.x = local_x + data->rects.window.left;
    screen.y = local_y + data->rects.window.top;
    win_data_release(data);

    input.type = INPUT_MOUSE;
    input.mi.dx = screen.x;
    input.mi.dy = screen.y;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;

    TRACE("hwnd=%p local=%d,%d screen=%d,%d\n", hwnd, local_x, local_y,
          (int)screen.x, (int)screen.y);
    NtUserSendHardwareInput(hwnd, 0, &input, 0);
}

void winedrm_button(HWND hwnd, unsigned int button, BOOL pressed)
{
    INPUT input = {0};

    input.type = INPUT_MOUSE;
    switch (button)
    {
    case BTN_LEFT:   input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; break;
    case BTN_RIGHT:  input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN; break;
    case BTN_MIDDLE: input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN; break;
    case BTN_SIDE:
    case BTN_BACK:
        input.mi.dwFlags = MOUSEEVENTF_XDOWN;
        input.mi.mouseData = XBUTTON1;
        break;
    case BTN_EXTRA:
    case BTN_FORWARD:
        input.mi.dwFlags = MOUSEEVENTF_XDOWN;
        input.mi.mouseData = XBUTTON2;
        break;
    default:
        return;
    }
    /* The *UP flag is the *DOWN flag shifted left by one for every button. */
    if (!pressed) input.mi.dwFlags <<= 1;

    TRACE("hwnd=%p button=%#x pressed=%d\n", hwnd, button, pressed);
    NtUserSendHardwareInput(hwnd, 0, &input, 0);
}
