/*
 * winedrm.drv — the Wine graphics driver for the CrystallineLattice display
 * server (glacier).  Unix-side master header.
 *
 * This is the client side of CrystallineLattice (DESIGN.md §3.3, "Path β").
 * It maps Win32 windows onto server-owned glacier windows over a SOCK_SEQPACKET
 * socket, submits content as shm buffers (CL_COMMIT), and turns server events
 * (CL_CONFIGURE/FOCUS/CLOSE/INPUT) into Win32 messages.  Modeled on
 * winewayland.drv; the backend speaks CrystallineLattice instead of Wayland.
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

#ifndef __WINE_WINEDRM_LATTICE_H
#define __WINE_WINEDRM_LATTICE_H

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#include <pthread.h>

#include "windef.h"
#include "winbase.h"
#include "ntgdi.h"
#include "wine/gdi_driver.h"
#include "wine/list.h"
#include "wine/rbtree.h"

#include "unixlib.h"
#include "protocol.h"

/* We only use 4-byte (XRGB8888) formats. */
#define WINEDRM_BYTES_PER_PIXEL 4

/* Driver-internal window messages, posted from the event thread and handled on
 * the owning window's thread in WINEDRM_WindowMessage (cf. winewayland.drv). */
enum winedrm_window_message
{
    WM_WINEDRM_CONFIGURE = WM_WINE_FIRST_DRIVER_MSG,
    WM_WINEDRM_SET_FOREGROUND,
    WM_WINEDRM_DISPLAYCHANGE,   /* glacier changed resolution; refit the desktop */
};

/**********************************************************************
 *          Globals
 */

extern char *process_name;

/* Process-global connection to glacier. One per Wine process. */
struct lattice
{
    BOOL            initialized;
    int             fd;             /* the SOCK_SEQPACKET socket to glacier */
    uint32_t        version;        /* negotiated from CL_WELCOME */
    uint32_t        reconnect_token;/* from CL_WELCOME; presented to reclaim windows */
    int             screen_w;       /* virtual-screen size from CL_WELCOME */
    int             screen_h;
    LONG            next_wid;       /* monotonic client-scoped window-id source */
    pthread_mutex_t send_mutex;     /* serializes cl_send across threads */
};
extern struct lattice process_lattice;

/* A committable content buffer: an XRGB8888 memfd, mmap'd for the GDI blit and
 * passed to glacier via SCM_RIGHTS on each CL_COMMIT. */
struct drm_shm_buffer
{
    int      fd;            /* memfd; sent (not consumed) on every commit */
    void    *map;           /* mmap of fd, PROT_READ|PROT_WRITE */
    size_t   map_size;
    int      width, height;
    int      stride;        /* bytes; width * WINEDRM_BYTES_PER_PIXEL */
    LONG     ref;
};

/* Per-window driver data, keyed by HWND. */
struct drm_win_data
{
    struct rb_entry     entry;
    HWND                hwnd;
    uint32_t            wid;        /* 0 until CL_CREATE_WINDOW sent */
    enum cl_role        role;
    struct window_rects rects;      /* as last known to win32u */
    BOOL                created;    /* CL_CREATE_WINDOW sent for this wid */
    BOOL                managed;    /* server-managed toplevel */
    struct drm_shm_buffer *contents;/* last buffer committed for this window */
    /* The geometry glacier last knows about (from a create, a CL_CONFIGURE we
     * applied, or a CL_SET_GEOMETRY we sent). WindowPosChanged compares against
     * it so a Wine-initiated move/resize is forwarded, but the echo from
     * applying a server CONFIGURE is not. */
    RECT                last_server_rect;
    /* Last server-decided geometry (CL_CONFIGURE), applied on the window's own
     * thread in WINEDRM_WindowMessage(WM_WINEDRM_CONFIGURE). */
    struct { int x, y, w, h; BOOL valid; } pending;
};

/**********************************************************************
 *          Connection (lattice.c)
 */

BOOL lattice_process_init(void);
int  lattice_send(const void *msg, size_t len, const int *fds, int nfds);
uint32_t lattice_alloc_wid(void);

/* Re-establish the link after glacier drops/restarts, presenting our token to
 * reclaim windows. Retries briefly; FALSE if glacier stays gone. Event thread. */
BOOL lattice_reconnect(void);

/* Re-announce every live window (CREATE + TITLE + COMMIT) to a freshly
 * (re)connected server. Implemented in window.c; called after lattice_reconnect. */
void winedrm_replay_windows(void);

/**********************************************************************
 *          Event loop (event.c)
 */

void winedrm_dispatch_events(void);

/**********************************************************************
 *          Window model (window.c)
 */

struct drm_win_data *win_data_get(HWND hwnd);
struct drm_win_data *win_data_get_nolock(HWND hwnd);
void win_data_release(struct drm_win_data *data);
struct drm_win_data *win_data_from_wid(uint32_t wid); /* event thread; takes the lock */

/* Ensure a CL window exists for a visible managed HWND, then submit `buffer`
 * as its contents (CL_COMMIT).  Returns TRUE if a commit was sent. */
BOOL winedrm_commit(HWND hwnd, struct drm_shm_buffer *buffer);

/**********************************************************************
 *          Input injection (pointer.c / keyboard.c)
 */

void winedrm_motion(HWND hwnd, int local_x, int local_y);
void winedrm_button(HWND hwnd, unsigned int button, BOOL pressed);
void winedrm_key(HWND hwnd, uint32_t keysym, BOOL pressed);
void WINEDRM_SetCursor(HWND hwnd, HCURSOR hcursor);

/**********************************************************************
 *          SHM buffers (shmbuffer.c)
 */

struct drm_shm_buffer *drm_shm_buffer_create(int width, int height);
void drm_shm_buffer_ref(struct drm_shm_buffer *buffer);
void drm_shm_buffer_unref(struct drm_shm_buffer *buffer);

/**********************************************************************
 *          Helpers
 */

static inline BOOL intersect_rect(RECT *dst, const RECT *src1, const RECT *src2)
{
    dst->left = max(src1->left, src2->left);
    dst->top = max(src1->top, src2->top);
    dst->right = min(src1->right, src2->right);
    dst->bottom = min(src1->bottom, src2->bottom);
    return !IsRectEmpty(dst);
}

static inline LRESULT send_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    return NtUserMessageCall(hwnd, msg, wparam, lparam, NULL, NtUserSendMessage, FALSE);
}

/**********************************************************************
 *          USER driver entry points
 */

void    WINEDRM_DestroyWindow(HWND hwnd);
LRESULT WINEDRM_DesktopWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void    WINEDRM_SetWindowText(HWND hwnd, LPCWSTR text);
LRESULT WINEDRM_WindowMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
BOOL    WINEDRM_WindowPosChanging(HWND hwnd, UINT swp_flags, BOOL shaped, const struct window_rects *rects);
void    WINEDRM_WindowPosChanged(HWND hwnd, HWND insert_after, HWND owner_hint, UINT swp_flags,
                                 const struct window_rects *new_rects, struct window_surface *surface);
BOOL    WINEDRM_CreateWindowSurface(HWND hwnd, BOOL layered, const RECT *surface_rect, struct window_surface **surface);
UINT    WINEDRM_UpdateDisplayDevices(const struct gdi_device_manager *device_manager, void *param);

#endif /* __WINE_WINEDRM_LATTICE_H */
