/*
 * winedrm.drv — window surface (paint target → CrystallineLattice buffer)
 *
 * win32u paints a window's contents into the surface bitmap and calls flush();
 * we copy those pixels into the window's shm buffer and submit it to glacier
 * (CL_COMMIT).  M1 is single-buffered and copies the changed area each flush;
 * a buffer pool + dma-buf/fencing arrive in M4 (SPEC §8.3/§8.4).
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

struct drm_window_surface
{
    struct window_surface header;
    struct drm_shm_buffer *buffer;
};

static struct drm_window_surface *surface_cast(struct window_surface *surface)
{
    return (struct drm_window_surface *)surface;
}

static void drm_window_surface_set_clip(struct window_surface *surface, const RECT *rects, UINT count)
{
    /* glacier clips against the server-owned window geometry; nothing to do. */
}

static BOOL drm_window_surface_flush(struct window_surface *window_surface, const RECT *rect, const RECT *dirty,
                                     const BITMAPINFO *color_info, const void *color_bits, BOOL shape_changed,
                                     const BITMAPINFO *shape_info, const void *shape_bits)
{
    struct drm_window_surface *surface = surface_cast(window_surface);
    struct drm_shm_buffer *buffer = surface->buffer;
    int src_w = color_info->bmiHeader.biWidth;
    int src_h = abs(color_info->bmiHeader.biHeight);
    int src_stride = src_w * WINEDRM_BYTES_PER_PIXEL;
    int copy_w, copy_h, y;

    if (!buffer) return FALSE;

    copy_w = min(src_w, buffer->width);
    copy_h = min(src_h, buffer->height);

    /* color_bits is top-down (we request biHeight < 0 at create time), so rows
     * map straight across.  M1 copies the full overlap; per-dirty-rect copy is
     * an M4 optimization. */
    for (y = 0; y < copy_h; y++)
    {
        memcpy((char *)buffer->map + (size_t)y * buffer->stride,
               (const char *)color_bits + (size_t)y * src_stride,
               (size_t)copy_w * WINEDRM_BYTES_PER_PIXEL);
    }

    TRACE("hwnd %p rect %s dirty %s -> buffer %p\n", window_surface->hwnd,
          wine_dbgstr_rect(rect), wine_dbgstr_rect(dirty), buffer);

    return winedrm_commit(window_surface->hwnd, buffer);
}

static void drm_window_surface_destroy(struct window_surface *window_surface)
{
    struct drm_window_surface *surface = surface_cast(window_surface);

    TRACE("surface=%p\n", surface);
    if (surface->buffer) drm_shm_buffer_unref(surface->buffer);
}

static const struct window_surface_funcs drm_window_surface_funcs =
{
    drm_window_surface_set_clip,
    drm_window_surface_flush,
    drm_window_surface_destroy,
};

static struct window_surface *drm_window_surface_create(HWND hwnd, const RECT *rect)
{
    char info_buf[FIELD_OFFSET(BITMAPINFO, bmiColors[256])];
    BITMAPINFO *info = (BITMAPINFO *)info_buf;
    struct window_surface *window_surface;
    struct drm_window_surface *surface;
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;

    TRACE("hwnd %p rect %s\n", hwnd, wine_dbgstr_rect(rect));

    memset(info, 0, sizeof(*info));
    info->bmiHeader.biSize        = sizeof(info->bmiHeader);
    info->bmiHeader.biWidth       = width;
    info->bmiHeader.biHeight      = -height; /* top-down */
    info->bmiHeader.biPlanes      = 1;
    info->bmiHeader.biBitCount    = 32;
    info->bmiHeader.biSizeImage   = width * height * WINEDRM_BYTES_PER_PIXEL;
    info->bmiHeader.biCompression = BI_RGB;

    if (!(window_surface = window_surface_create(sizeof(*surface), &drm_window_surface_funcs,
                                                 hwnd, rect, info, 0)))
        return NULL;

    surface = surface_cast(window_surface);
    surface->buffer = drm_shm_buffer_create(width, height);
    if (!surface->buffer)
    {
        window_surface_release(window_surface);
        return NULL;
    }

    return window_surface;
}

BOOL WINEDRM_CreateWindowSurface(HWND hwnd, BOOL layered, const RECT *surface_rect, struct window_surface **surface)
{
    struct window_surface *previous;
    struct drm_win_data *data;

    TRACE("hwnd %p layered %u rect %s surface %p\n", hwnd, layered,
          wine_dbgstr_rect(surface_rect), surface);

    if ((previous = *surface) && previous->funcs == &drm_window_surface_funcs) return TRUE;
    if (!(data = win_data_get(hwnd))) return TRUE; /* use the default surface */
    if (previous) window_surface_release(previous);

    *surface = drm_window_surface_create(hwnd, surface_rect);

    win_data_release(data);
    return TRUE;
}
