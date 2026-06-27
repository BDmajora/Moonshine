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

#include <stdlib.h>

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

/* Extract a cursor's pixels as straight-alpha ARGB8888 (top-down) into a fresh
 * shm buffer. Returns the buffer (caller unrefs) + hotspot, or NULL on failure
 * (e.g. a monochrome cursor, which we don't render yet). Mirrors the Win32→ARGB
 * conversion winewayland.drv does for its cursor surface. */
static struct drm_shm_buffer *cursor_to_buffer(HCURSOR hcursor, int *hx, int *hy)
{
    char infobuf[FIELD_OFFSET(BITMAPINFO, bmiColors[256])];
    BITMAPINFO *bi = (BITMAPINFO *)infobuf;
    struct drm_shm_buffer *buf = NULL;
    ICONINFO info = {0};
    BITMAP bm;
    HDC hdc = 0;
    uint32_t *bits;
    BOOL ok = FALSE, has_alpha = FALSE;
    int i, j;

    if (!NtUserGetIconInfo(hcursor, &info, NULL, NULL, NULL, 0)) return NULL;
    *hx = info.xHotspot;
    *hy = info.yHotspot;
    if (!info.hbmColor) goto done;   /* monochrome cursor: unsupported for now */
    if (!NtGdiExtGetObjectW(info.hbmColor, sizeof(bm), &bm)) goto done;
    if (bm.bmWidth <= 0 || bm.bmHeight <= 0 ||
        bm.bmWidth > 256 || bm.bmHeight > 256) goto done;

    if (!(buf = drm_shm_buffer_create(bm.bmWidth, bm.bmHeight))) goto done;
    bits = buf->map;

    memset(bi, 0, sizeof(*bi));
    bi->bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi->bmiHeader.biWidth       = bm.bmWidth;
    bi->bmiHeader.biHeight      = -bm.bmHeight;   /* top-down */
    bi->bmiHeader.biPlanes      = 1;
    bi->bmiHeader.biBitCount    = 32;
    bi->bmiHeader.biCompression = BI_RGB;
    bi->bmiHeader.biSizeImage   = bm.bmWidth * bm.bmHeight * 4;

    hdc = NtGdiCreateCompatibleDC(0);
    if (!NtGdiGetDIBitsInternal(hdc, info.hbmColor, 0, bm.bmHeight, bits, bi,
                                DIB_RGB_COLORS, 0, 0))
        goto done;

    for (i = 0; i < bm.bmWidth * bm.bmHeight; i++)
        if ((has_alpha = (bits[i] & 0xff000000) != 0)) break;

    if (!has_alpha && info.hbmMask) {
        /* No alpha channel: derive it from the 1-bpp AND mask. */
        unsigned int width_bytes = (bm.bmWidth + 31) / 32 * 4;
        unsigned char *mask = malloc((size_t)width_bytes * bm.bmHeight);
        if (mask) {
            bi->bmiHeader.biBitCount  = 1;
            bi->bmiHeader.biSizeImage = width_bytes * bm.bmHeight;
            if (NtGdiGetDIBitsInternal(hdc, info.hbmMask, 0, bm.bmHeight, mask,
                                       bi, DIB_RGB_COLORS, 0, 0)) {
                uint32_t *ptr = bits;
                for (i = 0; i < bm.bmHeight; i++)
                    for (j = 0; j < bm.bmWidth; j++, ptr++)
                        if (!((mask[i * width_bytes + j / 8] << (j % 8)) & 0x80))
                            *ptr |= 0xff000000;
            }
            free(mask);
        }
    }
    ok = TRUE;

done:
    if (hdc) NtGdiDeleteObjectApp(hdc);
    if (info.hbmColor) NtGdiDeleteObjectApp(info.hbmColor);
    if (info.hbmMask) NtGdiDeleteObjectApp(info.hbmMask);
    if (buf && !ok) { drm_shm_buffer_unref(buf); buf = NULL; }
    return buf;
}

/* Forward the app's cursor shape to glacier (which owns the cursor plane but
 * follows the shape, so I-beam / resize arrows show like on Windows). */
void WINEDRM_SetCursor(HWND hwnd, HCURSOR hcursor)
{
    struct cl_set_cursor sc = { .type = CL_SET_CURSOR };
    struct drm_shm_buffer *buf;
    int hx = 0, hy = 0;

    if (!hcursor) {
        sc.hide = 1;
        lattice_send(&sc, sizeof(sc), NULL, 0);
        return;
    }

    if (!(buf = cursor_to_buffer(hcursor, &hx, &hy))) return;
    sc.width = buf->width;
    sc.height = buf->height;
    sc.hotspot_x = hx;
    sc.hotspot_y = hy;
    lattice_send(&sc, sizeof(sc), &buf->fd, 1);
    drm_shm_buffer_unref(buf);
}
