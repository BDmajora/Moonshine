/*
 * winedrm.drv — window handling (HWND ↔ CrystallineLattice window)
 *
 * Maps each managed top-level HWND to a server-owned glacier window.  The
 * client only *requests* role and geometry (CL_CREATE_WINDOW); glacier decides
 * placement, z-order and focus and reports them back (CL_CONFIGURE/FOCUS).
 * Structure follows winewayland.drv's window.c.
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

#include <assert.h>
#include <stdlib.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "lattice.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedrm);

static int win_data_cmp_rb(const void *key, const struct rb_entry *entry)
{
    HWND key_hwnd = (HWND)key; /* cast to work around const */
    const struct drm_win_data *data = RB_ENTRY_VALUE(entry, const struct drm_win_data, entry);

    if (key_hwnd < data->hwnd) return -1;
    if (key_hwnd > data->hwnd) return 1;
    return 0;
}

static pthread_mutex_t win_data_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct rb_tree win_data_rb = { win_data_cmp_rb };

/***********************************************************************
 *           win_data_create
 *
 * Create the driver data for a window.  Returns with win_data_mutex held (the
 * caller releases via win_data_release); returns NULL, unlocked, for windows we
 * don't manage (desktop / HWND_MESSAGE).
 */
static struct drm_win_data *win_data_create(HWND hwnd, const struct window_rects *rects)
{
    struct drm_win_data *data;
    struct rb_entry *rb_entry;
    HWND parent;

    /* Don't create data for the desktop or for HWND_MESSAGE windows. */
    if (!(parent = NtUserGetAncestor(hwnd, GA_PARENT))) return NULL;
    if (parent != NtUserGetDesktopWindow() && !NtUserGetAncestor(parent, GA_PARENT))
        return NULL;

    if (!(data = calloc(1, sizeof(*data)))) return NULL;

    data->hwnd = hwnd;
    data->rects = *rects;

    pthread_mutex_lock(&win_data_mutex);

    /* Another thread may have created it meanwhile. */
    if ((rb_entry = rb_get(&win_data_rb, hwnd)))
    {
        free(data);
        return RB_ENTRY_VALUE(rb_entry, struct drm_win_data, entry);
    }

    rb_put(&win_data_rb, hwnd, &data->entry);
    TRACE("hwnd=%p\n", data->hwnd);
    return data;
}

struct drm_win_data *win_data_get_nolock(HWND hwnd)
{
    struct rb_entry *rb_entry;

    if ((rb_entry = rb_get(&win_data_rb, hwnd)))
        return RB_ENTRY_VALUE(rb_entry, struct drm_win_data, entry);
    return NULL;
}

struct drm_win_data *win_data_get(HWND hwnd)
{
    struct drm_win_data *data;

    pthread_mutex_lock(&win_data_mutex);
    if ((data = win_data_get_nolock(hwnd))) return data;
    pthread_mutex_unlock(&win_data_mutex);
    return NULL;
}

void win_data_release(struct drm_win_data *data)
{
    assert(data);
    pthread_mutex_unlock(&win_data_mutex);
}

struct drm_win_data *win_data_from_wid(uint32_t wid)
{
    struct drm_win_data *data;

    pthread_mutex_lock(&win_data_mutex);
    RB_FOR_EACH_ENTRY(data, &win_data_rb, struct drm_win_data, entry)
    {
        if (data->created && data->wid == wid) return data;
    }
    pthread_mutex_unlock(&win_data_mutex);
    return NULL;
}

/***********************************************************************
 *           role classification
 *
 * M1: every managed window is NORMAL.  M3 maps Shell_TrayWnd→TASKBAR,
 * Progman/desktop→DESKTOP, menus→MENU, tooltips→TOOLTIP (SPEC §10), which is
 * what retires the frostedglass is_taskbar()/is_desktop() heuristics.
 */
static enum cl_role role_for_window(HWND hwnd)
{
    return CL_ROLE_NORMAL;
}

/* ---- "managed" classification, ported from winewayland.drv ------------- */

static BOOL is_managed(HWND hwnd)
{
    struct drm_win_data *data = win_data_get(hwnd);
    BOOL ret = data && data->managed;
    if (data) win_data_release(data);
    return ret;
}

static HWND *build_hwnd_list(void)
{
    NTSTATUS status;
    HWND *list;
    ULONG count = 128;

    for (;;)
    {
        if (!(list = malloc(count * sizeof(*list)))) return NULL;
        status = NtUserBuildHwndList(0, 0, 0, 0, 0, count, list, &count);
        if (!status) return list;
        free(list);
        if (status != STATUS_BUFFER_TOO_SMALL) return NULL;
    }
}

static BOOL has_owned_popups(HWND hwnd)
{
    HWND *list;
    UINT i;
    BOOL ret = FALSE;

    if (!(list = build_hwnd_list())) return FALSE;

    for (i = 0; list[i] != HWND_BOTTOM; i++)
    {
        if (list[i] == hwnd) break;  /* popups are always above owner */
        if (NtUserGetWindowRelative(list[i], GW_OWNER) != hwnd) continue;
        if ((ret = is_managed(list[i]))) break;
    }

    free(list);
    return ret;
}

static inline HWND get_active_window(void)
{
    GUITHREADINFO info;
    info.cbSize = sizeof(info);
    return NtUserGetGUIThreadInfo(GetCurrentThreadId(), &info) ? info.hwndActive : 0;
}

static BOOL is_window_managed(HWND hwnd, UINT swp_flags, BOOL fullscreen)
{
    DWORD style, ex_style;

    /* child windows are not managed */
    style = NtUserGetWindowLongW(hwnd, GWL_STYLE);
    if ((style & (WS_CHILD|WS_POPUP)) == WS_CHILD) return FALSE;
    /* activated windows are managed */
    if (!(swp_flags & (SWP_NOACTIVATE|SWP_HIDEWINDOW))) return TRUE;
    if (hwnd == get_active_window()) return TRUE;
    /* windows with caption are managed */
    if ((style & WS_CAPTION) == WS_CAPTION) return TRUE;
    /* windows with thick frame are managed */
    if (style & WS_THICKFRAME) return TRUE;
    if (style & WS_POPUP)
    {
        /* popup with sysmenu == caption are managed */
        if (style & WS_SYSMENU) return TRUE;
        /* full-screen popup windows are managed */
        if (fullscreen) return TRUE;
    }
    /* application windows are managed */
    ex_style = NtUserGetWindowLongW(hwnd, GWL_EXSTYLE);
    if (ex_style & WS_EX_APPWINDOW) return TRUE;
    /* windows that own popups are managed */
    if (has_owned_popups(hwnd)) return TRUE;
    /* default: not managed */
    return FALSE;
}

/* ---- CrystallineLattice window lifecycle ------------------------------- */

static int window_text_utf8(HWND hwnd, char *out, int out_size)
{
    WCHAR textW[CL_TITLE_MAX];
    DWORD utf8_len = 0;
    int lenW;

    out[0] = 0;
    lenW = NtUserInternalGetWindowText(hwnd, textW, ARRAY_SIZE(textW));
    if (lenW <= 0) return 0;

    if (RtlUnicodeToUTF8N(out, out_size - 1, &utf8_len, textW, lenW * sizeof(WCHAR)))
        return 0;
    if (utf8_len > (DWORD)(out_size - 1)) utf8_len = out_size - 1;
    out[utf8_len] = 0;
    return utf8_len;
}

/* Caller must hold win_data_mutex (i.e. came through win_data_get). */
static BOOL ensure_cl_window(struct drm_win_data *data)
{
    struct cl_create_window cw;
    struct cl_set_title st;
    DWORD style;
    int w, h;

    if (data->created) return TRUE;

    style = NtUserGetWindowLongW(data->hwnd, GWL_STYLE);
    if (!(style & WS_VISIBLE)) return FALSE;

    w = data->rects.window.right - data->rects.window.left;
    h = data->rects.window.bottom - data->rects.window.top;
    if (w <= 0 || h <= 0) return FALSE;

    data->wid = lattice_alloc_wid();
    data->role = role_for_window(data->hwnd);

    cw = (struct cl_create_window){
        .type = CL_CREATE_WINDOW, .wid = data->wid, .role = data->role,
        .x = data->rects.window.left, .y = data->rects.window.top,
        .w = w, .h = h,
    };
    if (lattice_send(&cw, sizeof(cw), NULL, 0) != 0)
    {
        data->wid = 0;
        return FALSE;
    }

    st = (struct cl_set_title){ .type = CL_SET_TITLE, .wid = data->wid };
    window_text_utf8(data->hwnd, st.title, sizeof(st.title));
    lattice_send(&st, sizeof(st), NULL, 0);

    data->created = TRUE;
    TRACE("hwnd=%p wid=%u role=%u %dx%d\n", data->hwnd, data->wid, data->role, w, h);
    return TRUE;
}

/* Caller must hold win_data_mutex. */
static void destroy_cl_window(struct drm_win_data *data)
{
    struct cl_destroy_window dw = { .type = CL_DESTROY_WINDOW, .wid = data->wid };

    if (!data->created) return;
    TRACE("hwnd=%p wid=%u\n", data->hwnd, data->wid);
    lattice_send(&dw, sizeof(dw), NULL, 0);
    data->created = FALSE;
    data->wid = 0;
    if (data->contents)
    {
        drm_shm_buffer_unref(data->contents);
        data->contents = NULL;
    }
}

BOOL winedrm_commit(HWND hwnd, struct drm_shm_buffer *buffer)
{
    struct drm_win_data *data;
    struct cl_commit cm;
    BOOL ret = FALSE;

    if (!buffer) return FALSE;
    if (!(data = win_data_get(hwnd))) return FALSE;

    if (ensure_cl_window(data))
    {
        cm = (struct cl_commit){
            .type = CL_COMMIT, .wid = data->wid,
            .buffer = {
                .kind = CL_BUF_SHM,
                .width = buffer->width, .height = buffer->height,
                .stride = buffer->stride, .format = CL_FORMAT_XRGB8888,
                .modifier = 0, .has_fence = 0,
            },
        };
        if (lattice_send(&cm, sizeof(cm), &buffer->fd, 1) == 0)
        {
            if (data->contents != buffer)
            {
                if (data->contents) drm_shm_buffer_unref(data->contents);
                drm_shm_buffer_ref(buffer);
                data->contents = buffer;
            }
            ret = TRUE;
        }
    }

    win_data_release(data);
    return ret;
}

/* ---- USER driver entry points ----------------------------------------- */

void WINEDRM_DestroyWindow(HWND hwnd)
{
    struct drm_win_data *data;

    TRACE("%p\n", hwnd);

    if (!(data = win_data_get(hwnd))) return;

    destroy_cl_window(data);
    rb_remove(&win_data_rb, &data->entry);
    pthread_mutex_unlock(&win_data_mutex);

    if (data->contents) drm_shm_buffer_unref(data->contents);
    free(data);
}

BOOL WINEDRM_WindowPosChanging(HWND hwnd, UINT swp_flags, BOOL shaped, const struct window_rects *rects)
{
    struct drm_win_data *data = win_data_get(hwnd);

    TRACE("hwnd %p swp %04x shaped %u rects %s\n", hwnd, swp_flags, shaped, debugstr_window_rects(rects));

    if (!data && !(data = win_data_create(hwnd, rects))) return FALSE;

    win_data_release(data);
    return TRUE;
}

void WINEDRM_WindowPosChanged(HWND hwnd, HWND insert_after, HWND owner_hint, UINT swp_flags,
                              const struct window_rects *new_rects, struct window_surface *surface)
{
    BOOL managed, fullscreen = swp_flags & WINE_SWP_FULLSCREEN;
    struct drm_win_data *data;

    TRACE("hwnd %p new_rects %s after %p flags %08x surface %p\n",
          hwnd, debugstr_window_rects(new_rects), insert_after, swp_flags, surface);

    /* Compute managed state with win_data unlocked: is_window_managed may query
     * other HWNDs' win_data and take the lock itself. */
    managed = is_window_managed(hwnd, swp_flags, fullscreen);

    if (!(data = win_data_get(hwnd))) return;

    data->rects = *new_rects;
    data->managed = managed;

    /* Hidden / surface-less windows are torn down on the server; visible ones
     * are (re)created lazily on the first content commit (winedrm_commit). */
    if ((!surface || !NtUserIsWindowVisible(hwnd)) && data->created)
        destroy_cl_window(data);

    win_data_release(data);
}

static void winedrm_apply_configure(HWND hwnd)
{
    struct drm_win_data *data;
    RECT rect;
    BOOL valid;
    UINT flags = SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER;

    if (!(data = win_data_get(hwnd))) return;
    valid = data->pending.valid;
    SetRect(&rect, data->pending.x, data->pending.y,
            data->pending.x + data->pending.w, data->pending.y + data->pending.h);
    data->pending.valid = FALSE;
    win_data_release(data);

    if (!valid || IsRectEmpty(&rect)) return;
    TRACE("hwnd %p -> %s\n", hwnd, wine_dbgstr_rect(&rect));
    NtUserSetRawWindowPos(hwnd, rect, flags, FALSE);
}

LRESULT WINEDRM_WindowMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_WINEDRM_CONFIGURE:
        winedrm_apply_configure(hwnd);
        return 0;
    case WM_WINEDRM_SET_FOREGROUND:
        NtUserSetForegroundWindowInternal(hwnd);
        return 0;
    default:
        FIXME("got driver window msg %x hwnd %p wp %lx lp %lx\n", msg, hwnd, (long)wp, lp);
        return 0;
    }
}

LRESULT WINEDRM_DesktopWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return NtUserMessageCall(hwnd, msg, wp, lp, 0, NtUserDefWindowProc, FALSE);
}

void WINEDRM_SetWindowText(HWND hwnd, LPCWSTR text)
{
    struct drm_win_data *data;
    struct cl_set_title st;
    DWORD utf8_len = 0;

    TRACE("hwnd=%p text=%s\n", hwnd, wine_dbgstr_w(text));

    if (!(data = win_data_get(hwnd))) return;

    if (data->created)
    {
        st = (struct cl_set_title){ .type = CL_SET_TITLE, .wid = data->wid };
        if (text && !RtlUnicodeToUTF8N(st.title, sizeof(st.title) - 1, &utf8_len,
                                       text, lstrlenW(text) * sizeof(WCHAR)))
        {
            if (utf8_len > sizeof(st.title) - 1) utf8_len = sizeof(st.title) - 1;
            st.title[utf8_len] = 0;
        }
        lattice_send(&st, sizeof(st), NULL, 0);
    }

    win_data_release(data);
}
