/*
 * winedrm.drv — server→client event dispatch
 *
 * Runs on a dedicated thread (winedrm_unix_func_read_events) and turns glacier
 * events into Win32 traffic.  Geometry/focus changes are stashed in the window
 * data and applied on the owning window's thread via posted driver messages,
 * mirroring winewayland.drv's WM_WAYLAND_* pattern.
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

#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "lattice.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedrm);

static void handle_configure(const struct cl_configure *cfg)
{
    struct drm_win_data *data;
    HWND hwnd;

    if (!(data = win_data_from_wid(cfg->wid))) return;
    data->pending.x = cfg->x;
    data->pending.y = cfg->y;
    data->pending.w = cfg->w;
    data->pending.h = cfg->h;
    data->pending.valid = TRUE;
    hwnd = data->hwnd;
    win_data_release(data);

    TRACE("wid %u -> hwnd %p configure %d,%d %ux%u\n", cfg->wid, hwnd,
          cfg->x, cfg->y, cfg->w, cfg->h);
    NtUserPostMessage(hwnd, WM_WINEDRM_CONFIGURE, 0, 0);
}

static void handle_focus(const struct cl_focus *focus)
{
    struct drm_win_data *data;
    HWND hwnd;

    if (!(data = win_data_from_wid(focus->wid))) return;
    hwnd = data->hwnd;
    win_data_release(data);

    TRACE("wid %u -> hwnd %p focused=%u\n", focus->wid, hwnd, focus->focused);
    if (focus->focused) NtUserPostMessage(hwnd, WM_WINEDRM_SET_FOREGROUND, 0, 0);
}

static void handle_close(const struct cl_close *close_msg)
{
    struct drm_win_data *data;
    HWND hwnd;

    if (!(data = win_data_from_wid(close_msg->wid))) return;
    hwnd = data->hwnd;
    win_data_release(data);

    TRACE("wid %u -> hwnd %p close\n", close_msg->wid, hwnd);
    NtUserPostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
}

static void handle_screen(const struct cl_screen *s)
{
    process_lattice.screen_w = s->w;
    process_lattice.screen_h = s->h;
    TRACE("virtual screen → %ux%u\n", s->w, s->h);
    /* Apply on a GUI thread: it re-reads display devices and resizes the Wine
     * virtual desktop, which broadcasts WM_DISPLAYCHANGE so the taskbar and
     * wallpaper refit. */
    NtUserPostMessage(NtUserGetDesktopWindow(), WM_WINEDRM_DISPLAYCHANGE, 0, 0);
}

static void handle_input(const struct cl_input *in)
{
    struct drm_win_data *data;
    HWND hwnd;

    if (!(data = win_data_from_wid(in->wid))) return;
    hwnd = data->hwnd;
    win_data_release(data);

    switch (in->kind)
    {
    case CL_IN_MOTION: winedrm_motion(hwnd, in->x, in->y); break;
    case CL_IN_BUTTON: winedrm_button(hwnd, in->code, in->pressed); break;
    case CL_IN_KEY:    winedrm_key(hwnd, in->code, in->pressed); break;
    default:           WARN("unknown input kind %u\n", in->kind); break;
    }
}

/***********************************************************************
 *           winedrm_dispatch_events
 *
 * Blocking receive loop.  Returns only when the link to glacier drops or a
 * fatal error occurs, at which point the caller terminates the process.
 */
void winedrm_dispatch_events(void)
{
    char buf[512];
    int fds[CL_MAX_FDS], nfds;
    ssize_t n;

    for (;;)
    {
        nfds = 0;
        n = cl_recv(process_lattice.fd, buf, sizeof(buf), fds, &nfds);
        /* Server→client messages carry no fds today; close any defensively. */
        while (nfds-- > 0) close(fds[nfds]);

        if (n <= 0)
        {
            if (n == 0) TRACE("glacier closed the connection\n");
            else ERR("cl_recv error\n");
            return;
        }
        if (n < (ssize_t)sizeof(uint32_t)) continue;

        switch (*(uint32_t *)buf)
        {
        case CL_CONFIGURE:
            if (n >= (ssize_t)sizeof(struct cl_configure))
                handle_configure((struct cl_configure *)buf);
            break;
        case CL_FOCUS:
            if (n >= (ssize_t)sizeof(struct cl_focus))
                handle_focus((struct cl_focus *)buf);
            break;
        case CL_CLOSE:
            if (n >= (ssize_t)sizeof(struct cl_close))
                handle_close((struct cl_close *)buf);
            break;
        case CL_INPUT:
            if (n >= (ssize_t)sizeof(struct cl_input))
                handle_input((struct cl_input *)buf);
            break;
        case CL_SCREEN:
            if (n >= (ssize_t)sizeof(struct cl_screen))
                handle_screen((struct cl_screen *)buf);
            break;
        default:
            WARN("unexpected message type %u\n", *(uint32_t *)buf);
            break;
        }
    }
}
