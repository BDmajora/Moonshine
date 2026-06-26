# winedrm.drv — design & specification

**Status:** spec / pre-implementation
**Component of:** Moonshine (Wine fork) — the Wine graphics driver for the
CrystallineLattice display server (glacier).
**Replaces:** `winewayland.drv` (and, transitively, the frostedglass hack layer).
**Models on:** `winewayland.drv` (PE/unix split, USER_DRIVER vtable, buffer flow)
and `winex11.drv` (window/event/keyboard structure).
**Speaks:** the CrystallineLattice native wire format (Path β), defined in the
glacier repo at `include/protocol.h`. See CrystallineLattice `DESIGN.md` §3.3.

> This is the *client side* of CrystallineLattice. glacier is the server; this
> driver is what every Win32 process loads to put windows on glacier's screen,
> take input, and integrate with the server-authoritative shell. The reference
> for "a correct minimal client" is glacier's own `src/client.c`
> (`glacier-client`) — winedrm.drv is that client, wearing the Wine USER_DRIVER
> interface and scaled up to real apps.

---

## 0. Purpose & scope

The Wine USER/GDI subsystem (`win32u`) talks to the host display system through a
single pluggable `struct user_driver_funcs` vtable. Today that vtable is filled
by `winewayland.drv` (Wayland) or `winex11.drv` (X11). winedrm.drv is a third
implementation whose backend is **not** a generic display protocol but the
purpose-built, server-authoritative CrystallineLattice protocol.

The driver's job, stated as four contracts:

1. **Windows → server windows.** Map each managed Win32 `HWND` to a
   CrystallineLattice window (`CL_CREATE_WINDOW` / `CL_DESTROY_WINDOW`), tagged
   with an explicit **role** (`NORMAL | DESKTOP | TASKBAR | TRAY | MENU |
   TOOLTIP`). The shell decides placement/z-order/focus; the driver only
   *requests*.
2. **Pixels → buffers.** Take the bitmap `win32u` hands us for a window's
   contents and submit it as a CrystallineLattice buffer (`CL_COMMIT`): an
   shm/memfd of `XRGB8888` first; a dma-buf/EGLImage with a render-fence later.
3. **Server events → Win32 messages.** Translate `CL_CONFIGURE`, `CL_FOCUS`,
   `CL_CLOSE`, and `CL_INPUT` into `WM_*` traffic: move/resize, activation,
   `WM_CLOSE`, mouse, and keyboard (keysym → VK + `WM_CHAR`).
4. **Stay out of policy.** No cursor ownership (glacier owns the KMS cursor
   plane), no decoration drawing (glacier draws SSD), no geometry heuristics
   (roles replace them). Every frostedglass hack this kills is listed in
   `DESIGN.md` §1.

**Out of scope (v1):** clipboard/data-device, drag-and-drop, IME beyond basic
dead keys, multi-GPU buffer routing, and the GL/Vulkan ICD fast path (stubbed
first, see §8.4 / §16).

---

## 1. Position in the Wine stack

### 1.1 How Wine loads it

`win32u` resolves the active graphics driver from the desktop window's
`GraphicsDriver` registry value and loads it via the `NtUserLoadDriver`
user-mode callback (`dlls/win32u/driver.c`). The candidate list comes from
`programs/explorer/desktop.c`:

```c
static const WCHAR default_driver[] = L"mac,x11,wayland";   /* today */
```

winedrm.drv is added to this list (and, on a YetiOS/glacier image, becomes the
only entry):

```c
static const WCHAR default_driver[] = L"mac,x11,wayland,drm"; /* transition */
/* on a glacier-only image: L"drm" */
```

Each candidate `foo` loads `winefoo.drv`; so `drm` ⇒ `winedrm.drv`. The module
name and `MODULE`/`UNIXLIB` in `Makefile.in` must therefore be `winedrm.drv` /
`winedrm.so`.

### 1.2 The PE / unix split (mandatory)

Like every modern Wine driver, winedrm.drv is built as two halves:

- **PE side** (`dllmain.c`, `winedrm_dll.h`) — runs as guest Win32 code.
  Hosts `DllMain`, spawns helper threads, and reaches the unix side only through
  `WINE_UNIX_CALL`.
- **Unix side** (everything tagged `#pragma makedep unix`) — native ELF that may
  call libc, sockets, and `win32u`. Registers the USER_DRIVER vtable via
  `__wine_set_user_driver(&winedrm_funcs, WINE_GDI_DRIVER_VERSION)`.

The boundary is the `enum winedrm_unix_func` table in `unixlib.h`, invoked with
`WINEDRM_UNIX_CALL(func, params)` (mirrors `WAYLANDDRV_UNIX_CALL`). Pointers do
not cross the boundary except as 64-bit-safe integer fields in param structs
(see the `set_boot_cursor_params` precedent — though winedrm has no boot cursor;
§11).

### 1.3 What it inherits, keeps, drops vs. the existing drivers

| Concern | winex11.drv | winewayland.drv | **winedrm.drv** |
|---|---|---|---|
| Transport | Xlib | libwayland-client + xdg-shell | **bare `AF_UNIX` SEQPACKET** + CrystallineLattice protocol |
| Window handles | X Window | `wl_surface` + `xdg_toplevel` | **`cl` window id** (client-scoped uint32) |
| Geometry authority | WM (negotiated) | compositor (negotiated) | **server-authoritative** (`CL_CONFIGURE`) |
| Decorations | WM-drawn | CSD/SSD negotiation | **server-drawn, always** (no negotiation) |
| Cursor | X cursor | `wl_pointer.set_cursor` / yetios mgr | **none — server owns KMS plane** |
| Buffers | XShm / GLX | `wl_shm` / dmabuf | **shm/memfd now, dma-buf next** (`SCM_RIGHTS`) |
| Keyboard | XKB→VK in driver | xkb keymap→KBDTABLES in driver | **keysym→VK in driver** (glacier ships keysym) |
| Roles | heuristic | heuristic + tiled-hint hack | **explicit `CL_CREATE_WINDOW.role`** |
| Event source | `XNextEvent` | `wl_display_dispatch` | **`cl_recv` loop on the client fd** |

The structural shape (a `*_win_data` rbtree keyed by HWND, a per-window surface
object, an event-reader thread, a USER_DRIVER vtable) is lifted from
winewayland.drv. The *backend* of every method is rewritten to speak
CrystallineLattice.

---

## 2. The protocol it speaks (CrystallineLattice v0)

Defined in glacier `include/protocol.h`. The driver **vendors a copy** of that
header and `protocol.c` (`cl_send`/`cl_recv`/`cl_default_socket_path`) so the two
repos can build independently; the copy must stay byte-compatible (same
`CL_MAGIC`, `CL_VERSION`, `CL_MIN_VERSION`, struct layouts). See §13.3 for how
the copy is kept honest.

**Transport:** one `SOCK_SEQPACKET` `AF_UNIX` socket per process, connected to
`$GLACIER_SOCKET` (else `$XDG_RUNTIME_DIR/glacier-0`), resolved by
`cl_default_socket_path()`. SEQPACKET ⇒ one `sendmsg` == one `recvmsg`; framing
is free. fds ride as `SCM_RIGHTS` ancillary data (`CL_MAX_FDS == 2`: buffer
[+ render-fence]).

**Messages the driver SENDS (client → server):**

| Message | When | Carries |
|---|---|---|
| `CL_HELLO` | once, immediately after connect | `magic`, `version`, `min_version` |
| `CL_CREATE_WINDOW` | a managed HWND becomes visible | `wid`, `role`, requested `x,y,w,h` |
| `CL_SET_TITLE` | `SetWindowText` / create | `wid`, `title[CL_TITLE_MAX]` (UTF-8, ≤63) |
| `CL_COMMIT` | window contents flushed | `wid`, `struct cl_buffer` + fd(s) via SCM_RIGHTS |
| `CL_DESTROY_WINDOW` | HWND destroyed/hidden | `wid` |

**Messages the driver RECEIVES (server → client):**

| Message | Driver reaction |
|---|---|
| `CL_WELCOME` | record negotiated `version` + virtual-screen `screen_w/h`; unblock init |
| `CL_CONFIGURE` | server-decided `x,y,w,h` → drive `SetWindowPos` / `WM_WINDOWPOSCHANGED` |
| `CL_FOCUS` | `focused` 1/0 → activate / deactivate (`WM_ACTIVATE`, foreground) |
| `CL_CLOSE` | shell asks window to close → post `WM_CLOSE` |
| `CL_INPUT` | `kind` ∈ {MOTION, BUTTON, KEY} → synthesize mouse/keyboard input |

`struct cl_buffer` already carries `format` (DRM fourcc), `modifier`, and a
`has_fence` flag, so the shm→dma-buf upgrade needs no wire change. `CL_INPUT`
already carries window-local `x,y` for motion, evdev `BTN_*` for buttons, and an
**xkb keysym** for keys — the driver does the keysym→VK mapping (§9).

---

## 3. File layout

Modeled on `winewayland.drv`, renamed and reduced to the CrystallineLattice
surface. Files marked **unix** carry `#pragma makedep unix`.

```
dlls/winedrm.drv/
  SPEC.md                  this document
  Makefile.in              MODULE=winedrm.drv, UNIXLIB=winedrm.so, sources, libs
  version.rc               version resource (clone of winewayland.drv/version.rc)

  winedrm_dll.h            PE-side header: WINEDRM_UNIX_CALL macro, includes unixlib.h
  unixlib.h                enum winedrm_unix_func + param structs (PE↔unix ABI)
  dllmain.c            PE  DllMain: __wine_init_unix_call, init, spawn event thread
  winedrm_main.c     unix  unixlib entry table + the user_driver_funcs vtable + init

  protocol.h              VENDORED from glacier include/protocol.h (keep in sync)
  protocol.c          unix VENDORED cl_send/cl_recv/cl_default_socket_path
  lattice.h               unix-side master header (the "winedrv.h" of this driver)
  lattice.c           unix connection: connect, HELLO/WELCOME handshake, send/recv,
                           reconnect; the global `struct lattice` (cf. process_wayland)
  event.c             unix the cl_recv dispatch loop (CONFIGURE/FOCUS/CLOSE/INPUT)

  window.c            unix HWND↔wid map (rbtree), roles, WindowPosChanging/Changed,
                           DestroyWindow, SetWindowText, SetWindowStyle, SysCommand
  window_surface.c    unix CreateWindowSurface + flush → buffer alloc → CL_COMMIT
  surface.c           unix per-window CL state: wid alloc, pending/current config
  shmbuffer.c         unix memfd/shm buffer pool (XRGB8888); later: gbm/dma-buf

  display.c           unix UpdateDisplayDevices from CL_WELCOME screen size; desktop
  pointer.c           unix CL_INPUT MOTION/BUTTON → mouse_event; SetCursor (no-op),
                           SetCursorPos (warp request, future)
  keyboard.c          unix CL_INPUT KEY: keysym→VK + WM_CHAR; KbdLayerDescriptor
  opengl.c            unix OpenGLInit — stub first; EGL/dma-buf path later (§8.4)
  vulkan.c            unix VulkanInit — stub first; WSI later (§8.4)
```

Compared with winewayland.drv we **drop**: `wayland_data_device.c`,
`wayland_text_input.c`, `wayland_output.c` (folded into `display.c`), and every
`*.xml` protocol file (CrystallineLattice is not extension-based). We **gain**:
`protocol.c`, `lattice.c`, `shmbuffer.c`.

---

## 4. Core data structures

Defined in `lattice.h` (the unix-side master header, the analogue of
`waylanddrv.h`).

```c
/* Process-global connection state. One per Wine process. cf. process_wayland. */
struct lattice
{
    BOOL            initialized;
    int             fd;             /* the SEQPACKET socket to glacier */
    uint32_t        version;        /* negotiated from CL_WELCOME */
    int             screen_w, screen_h;  /* virtual-screen size from WELCOME */
    uint32_t        next_wid;       /* monotonic client-scoped window-id source */
    pthread_mutex_t send_mutex;     /* serializes cl_send from any thread */
    struct rb_tree  windows;        /* wid → struct drm_win_data (event thread lookup) */
    pthread_mutex_t windows_mutex;
    LONG            input_serial;
};
extern struct lattice process_lattice;
extern char *process_name;

/* Per-window driver data, keyed by HWND (created on demand). cf. wayland_win_data. */
struct drm_win_data
{
    struct rb_entry entry;          /* by HWND */
    struct rb_entry wid_entry;      /* by wid, for event-thread reverse lookup */
    HWND            hwnd;
    uint32_t        wid;            /* 0 until CL_CREATE_WINDOW sent */
    enum cl_role    role;
    struct window_rects rects;      /* as last known to win32u */
    struct drm_window_config pending;   /* last CL_CONFIGURE not yet applied */
    BOOL            created;        /* CL_CREATE_WINDOW acknowledged-by-send */
    BOOL            mapped;         /* visible to the server */
    struct drm_shm_buffer *contents;/* last committed buffer */
};

/* A committable buffer. shm/memfd now; gains a gbm_bo / dmabuf fd + modifier. */
struct drm_shm_buffer
{
    int      fd;                    /* memfd; dup'd into CL_COMMIT via SCM_RIGHTS */
    void    *map;                   /* mmap for the GDI blit target */
    size_t   size;
    int      width, height, stride;
    uint32_t format;                /* CL_FORMAT_XRGB8888 */
    uint64_t modifier;              /* 0 (LINEAR/shm) */
    LONG     ref;
    BOOL     busy;                  /* server may still be sampling it */
};

/* Server-decided geometry awaiting application on the window thread. */
struct drm_window_config { RECT rect; BOOL focused; uint32_t serial; };
```

`struct window_surface` is subclassed exactly as winewayland.drv does it
(`struct drm_window_surface { struct window_surface header; ... }`) so `win32u`'s
generic surface machinery drives our flush.

---

## 5. The USER_DRIVER vtable

The unix side registers this table in `winedrm_main.c`:

```c
static const struct user_driver_funcs winedrm_funcs =
{
    .pUpdateDisplayDevices  = WINEDRM_UpdateDisplayDevices,
    .pCreateWindowSurface   = WINEDRM_CreateWindowSurface,
    .pWindowPosChanging     = WINEDRM_WindowPosChanging,
    .pWindowPosChanged      = WINEDRM_WindowPosChanged,
    .pDestroyWindow         = WINEDRM_DestroyWindow,
    .pSetWindowText         = WINEDRM_SetWindowText,
    .pSetWindowStyle        = WINEDRM_SetWindowStyle,
    .pWindowMessage         = WINEDRM_WindowMessage,
    .pDesktopWindowProc     = WINEDRM_DesktopWindowProc,
    .pSysCommand            = WINEDRM_SysCommand,
    .pSetCursor             = WINEDRM_SetCursor,      /* no-op: server owns cursor */
    .pSetCursorPos          = WINEDRM_SetCursorPos,   /* warp request (future) */
    .pKbdLayerDescriptor    = WINEDRM_KbdLayerDescriptor,
    .pReleaseKbdTables      = WINEDRM_ReleaseKbdTables,
    .pSetWindowIcons        = WINEDRM_SetWindowIcons, /* optional: CL_SET_TITLE has no icon yet */
    .pSetLayeredWindowAttributes = WINEDRM_SetLayeredWindowAttributes, /* later */
    .pVulkanInit            = WINEDRM_VulkanInit,     /* stub first */
    .pOpenGLInit            = WINEDRM_OpenGLInit,     /* stub first */
};
```

Function-by-function — signature follows `wine/gdi_driver.h`
`struct user_driver_funcs`; the "Does" column is the CrystallineLattice action.

| Driver function | Trigger | Does (CrystallineLattice action) |
|---|---|---|
| `WINEDRM_UpdateDisplayDevices(const struct gdi_device_manager*, void*)` | display refresh | report ONE monitor sized `screen_w × screen_h` from `CL_WELCOME` (multi-output later) |
| `WINEDRM_CreateWindowSurface(HWND, BOOL layered, const RECT*, struct window_surface**)` | window needs a paint target | allocate/resize a `drm_window_surface` backed by a `drm_shm_buffer` of the surface rect |
| `WINEDRM_WindowPosChanging(HWND, UINT swp, BOOL shaped, const struct window_rects*)` | geometry about to change | decide managed/role; ensure a `drm_win_data` exists; return whether we own the surface |
| `WINEDRM_WindowPosChanged(HWND, HWND after, HWND owner, UINT swp, const struct window_rects*, struct window_surface*)` | geometry changed / shown / hidden | first show ⇒ `CL_CREATE_WINDOW` (+`CL_SET_TITLE`); hide ⇒ `CL_DESTROY_WINDOW`; move/resize ⇒ remember rects (server is authoritative, so requests only) |
| `WINEDRM_DestroyWindow(HWND)` | `WM_NCDESTROY` | `CL_DESTROY_WINDOW`; free `drm_win_data`, drop buffers |
| `WINEDRM_SetWindowText(HWND, LPCWSTR)` | title change | UTF-16→UTF-8, truncate to `CL_TITLE_MAX-1`, `CL_SET_TITLE` |
| `WINEDRM_SetWindowStyle(HWND, INT, STYLESTRUCT*)` | style change | re-evaluate role (e.g. `WS_EX_TOOLWINDOW`, popup) and resend if changed |
| `WINEDRM_WindowMessage(HWND, UINT, WPARAM, LPARAM)` | internal driver msgs | handle `WM_WINEDRM_CONFIGURE` etc. posted from the event thread (apply server geometry on the window's own thread) |
| `WINEDRM_DesktopWindowProc(HWND, UINT, WPARAM, LPARAM)` | desktop window | size desktop to virtual screen; swallow background paint (glacier draws wallpaper) |
| `WINEDRM_SysCommand(HWND, WPARAM, LPARAM, const POINT*)` | `SC_MOVE`/`SC_SIZE` | (future) ask server for interactive move/resize; v1 returns -1 (let default run) |
| `WINEDRM_SetCursor(HWND, HCURSOR)` | cursor change | **no-op** — glacier owns the KMS cursor plane globally (§11) |
| `WINEDRM_SetCursorPos(INT, INT)` | `SetCursorPos` | (future) send a warp request; v1 best-effort/no-op, return TRUE |
| `WINEDRM_KbdLayerDescriptor(HKL)` / `WINEDRM_ReleaseKbdTables(const KBDTABLES*)` | layout query | provide a VK table consistent with the keysym→VK mapping (§9) |
| `WINEDRM_SetWindowIcons` / `WINEDRM_SetLayeredWindowAttributes` | icons / alpha | optional v1: store; act on once the protocol carries icon/alpha |
| `WINEDRM_VulkanInit` / `WINEDRM_OpenGLInit` | ICD bring-up | stub returning failure first; real EGL/dma-buf path in §8.4 |

**Deliberately absent** vs. winewayland.drv: `pClipboardWindowProc`,
`pClipCursor`, `pSetIMECompositionRect`. Clipboard and pointer confinement are
v2; the protocol has no message for them yet.

---

## 6. Internal functions per module

The non-vtable helpers the implementation will need, grouped by file. Names are
proposals; signatures sketch intent.

**lattice.c — connection & framed I/O**
- `BOOL lattice_process_init(void)` — connect, send `CL_HELLO`, await `CL_WELCOME`,
  populate `process_lattice`. Called from `winedrm_unix_init`. (cf.
  `wayland_process_init`.)
- `int lattice_send(const void *msg, size_t len, const int *fds, int nfds)` —
  `cl_send` under `send_mutex` (the event thread and window threads both send).
- `ssize_t lattice_recv(void *buf, size_t cap, int *fds, int *nfds)` — `cl_recv`
  wrapper used only by the event thread.
- `uint32_t lattice_alloc_wid(void)` — atomically bump `next_wid` (never 0).
- `BOOL lattice_reconnect(void)` — on link loss, attempt re-handshake + window
  re-creation (crash-resilience, §15).

**event.c — server→client dispatch**
- `void winedrm_dispatch_events(void)` — the blocking `lattice_recv` loop run on
  the event thread; switch on message type → the handlers below.
- `static void handle_configure(const struct cl_configure*)` — look up
  `drm_win_data` by wid, stash `pending`, post `WM_WINEDRM_CONFIGURE` to the
  window thread.
- `static void handle_focus(const struct cl_focus*)` — drive
  `NtUserSetForegroundWindow` / `WM_ACTIVATE`.
- `static void handle_close(const struct cl_close*)` — `NtUserPostMessage(hwnd,
  WM_CLOSE, ...)`.
- `static void handle_input(const struct cl_input*, int *fds, int nfds)` — route
  to `pointer.c` / `keyboard.c`.

**window.c — HWND lifecycle & roles**
- `struct drm_win_data *get_win_data(HWND)` / `release_win_data(...)` — rbtree
  lookup with the data lock held (cf. `wayland_win_data_get`).
- `struct drm_win_data *find_win_data_by_wid(uint32_t)` — reverse lookup for the
  event thread.
- `static enum cl_role role_for_window(HWND, DWORD style, DWORD exstyle)` — the
  function that **kills the heuristics** (§10).
- `static void create_cl_window(struct drm_win_data*)` — `CL_CREATE_WINDOW` +
  `CL_SET_TITLE`.
- `static void destroy_cl_window(struct drm_win_data*)` — `CL_DESTROY_WINDOW`.

**surface.c / window_surface.c / shmbuffer.c — the buffer path**
- `struct drm_shm_buffer *shm_buffer_create(int w, int h)` — `memfd_create` +
  `ftruncate` + `mmap` (cf. `glacier-client`'s `make_buffer`).
- `void shm_buffer_ref/unref(struct drm_shm_buffer*)`.
- `static void winedrm_surface_flush(struct window_surface*, const RECT*, const RECT*, ...)`
  — copy dirty bits into the buffer, then `commit_buffer`.
- `static void commit_buffer(struct drm_win_data*, struct drm_shm_buffer*)` —
  build `struct cl_commit`, `lattice_send(..., &fd, 1)` (or 2 with a fence).

**display.c**
- `static BOOL get_virtual_screen(RECT*)` — from `process_lattice.screen_w/h`.
- desktop proc helpers.

**pointer.c**
- `void winedrm_motion(HWND, int x, int y)` — `__wine_send_input` / mouse_event
  with absolute window-local coords mapped to virtual-screen.
- `void winedrm_button(HWND, uint32_t btn, BOOL pressed)` — evdev `BTN_*` →
  `MOUSEEVENTF_*`.

**keyboard.c**
- `void winedrm_key(HWND, uint32_t keysym, BOOL pressed, modifiers)` — §9.
- `static UINT keysym_to_vk(uint32_t keysym)` — the translation table.
- `static WCHAR keysym_to_char(uint32_t keysym, modifiers)` — for `WM_CHAR`,
  including dead-key composition state.

---

## 7. The unixlib boundary

`unixlib.h`:

```c
enum winedrm_unix_func
{
    winedrm_unix_func_init,         /* connect + handshake + register vtable */
    winedrm_unix_func_read_events,  /* the blocking event loop (own thread) */
    winedrm_unix_func_count,
};
/* No param structs needed for v1 (init/read_events take NULL), unlike
 * winewayland's set_boot_cursor — winedrm has no boot cursor (§11). Add structs
 * here only when a PE→unix call needs to pass data. */
```

`winedrm_main.c` exposes `__wine_unix_call_funcs[]` (and the WoW64 mirror) in the
order above, asserted against `winedrm_unix_func_count`.

`dllmain.c` (PE side), modeled on winewayland's `DllMain`:
1. `DisableThreadLibraryCalls`; `__wine_init_unix_call()`.
2. `WINEDRM_UNIX_CALL(init, NULL)` — fail the load if the server is unreachable
   (so Wine can fall back to the next driver in the list).
3. `CreateThread(... winedrm_read_events_thread ...)` — the thread calls
   `WINEDRM_UNIX_CALL(read_events, NULL)`, which only returns on fatal link loss;
   on return it `TerminateProcess` (matches winewayland's contract) unless
   reconnect (§15) is enabled.

**No clipboard thread** (v1 has no clipboard), and **no boot-cursor block** — the
single most visible deletion vs. winewayland's `DllMain`, and the concrete
realization of "the cursor is not Wine-provided anymore" (`DESIGN.md` §0.3).

---

## 8. Buffer / rendering path

### 8.1 Ownership (locked)
Per `DESIGN.md` §3.3: **client allocates, server imports.** winedrm.drv allocates
the buffer and hands glacier the fd; glacier imports whatever it is given. This
is already what `glacier-client` and the server transport do.

### 8.2 The shm path (v1)
`win32u` calls `WINEDRM_CreateWindowSurface`; we return a `drm_window_surface`
whose backing store is a `drm_shm_buffer` (`memfd`, `XRGB8888`, `stride = w*4`,
`modifier = 0`). GDI paints into the mmap. On flush we send `CL_COMMIT` with
`buffer.kind = CL_BUF_SHM`, passing the memfd via `SCM_RIGHTS`, `has_fence = 0`.
glacier mmaps and CPU-composites it (its current path). This reaches parity with
`glacier-client` but for a real Win32 app.

### 8.3 Double-buffering & release
glacier today does not send an explicit buffer-release. v1 keeps it simple: one
buffer per surface, reused; resize reallocates. When glacier gains a release
event (or we move to dma-buf with implicit/explicit sync), add a small pool and
honor `busy` (the `drm_shm_buffer.busy`/`ref` fields exist for this).

### 8.4 The dma-buf / GL path (v2)
`buffer.kind = CL_BUF_DMABUF`, real `format`/`modifier`, and `has_fence = 1` with
the render-fence fd as `fds[1]`. Driver-side this means a `gbm_device` on the DRM
render node, allocating scanout/render-capable bos, exporting the dma-buf fd, and
(for wined3d/DXVK) wiring `OpenGLInit`/`VulkanInit` to render into those bos with
an out-fence. The protocol already reserves every field this needs — no wire
change, just a `kind`/`has_fence` flip. This is where the §16 M4 work lands.

---

## 9. Input & keyboard

`CL_INPUT` arrives on the event thread with window-local `x,y` (motion), evdev
`BTN_*` (button), or an **xkb keysym** (key), plus `pressed`.

**Pointer** (`pointer.c`): motion is already window-local and in the global
virtual-screen space the server owns — feed it as absolute input. Map
`BTN_LEFT/RIGHT/MIDDLE` → `MOUSEEVENTF_LEFTDOWN/UP` etc.

**Keyboard** (`keyboard.c`): the locked decision (`DESIGN.md` §3.3) is that
**glacier ships keysyms and the driver maps keysym → Win32 VK + `WM_CHAR`**,
including dead-key/IME composition. This differs from winewayland.drv, which
receives the full xkb *keymap* and synthesizes `KBDTABLES`. Consequences to
design for:

- **Modifier tracking.** v0 `cl_input` carries only the keysym + `pressed`, not a
  modifier mask. The driver tracks Shift/Ctrl/Alt/CapsLock state itself by
  watching modifier *keysyms* (`XKB_KEY_Shift_L`, …) in the key stream, and
  derives the VK modifier state from that. **Decision to lock:** either keep
  deriving modifiers driver-side, or add a `mods` field to `cl_input` (a v1
  protocol bump — cheap, and the `min_version` handshake absorbs it). Recommended:
  add `mods` to avoid drift between server and driver modifier state.
- **keysym → VK.** A static table (`keysym_to_vk`) covering the printable Latin
  range, the function/navigation/numpad keys, and modifiers. The xkb keysym
  already reflects layout, so the table is layout-agnostic at the VK level.
- **`WM_CHAR`.** Derive the character from the keysym (Unicode keysyms map
  directly; `keysym - 0x01000000` for the U+ range), gated by a small dead-key
  composer for accents.
- **`KbdLayerDescriptor`.** Provide a minimal VK→scan/char table consistent with
  the above so apps that query the layout see something coherent. (Can start by
  borrowing the structure from winewayland's keyboard table generation and
  feeding it from our static map.)

---

## 10. Window roles (kills the heuristics)

`WINEDRM_WindowPosChanging`/`role_for_window` classify each managed window once
and pass the role in `CL_CREATE_WINDOW.role`. This is the single change that
deletes frostedglass's `is_taskbar()`/`is_desktop()` guessing (`DESIGN.md` §1).

| Win32 window | Detect by | `cl_role` |
|---|---|---|
| The shell taskbar | class `Shell_TrayWnd` (explorer) | `CL_ROLE_TASKBAR` |
| The desktop/wallpaper host | desktop window / `Progman` | `CL_ROLE_DESKTOP` |
| Tray/notification | tray classes | `CL_ROLE_TRAY` |
| Menus / popup menus | `WS_POPUP` + menu class | `CL_ROLE_MENU` |
| Tooltips | tooltip class / `WS_EX_TOOLWINDOW` transient | `CL_ROLE_TOOLTIP` |
| Everything else | default | `CL_ROLE_NORMAL` |

Role is resolved from window class + style/exstyle, never from geometry. If a
style change flips the classification, `WINEDRM_SetWindowStyle` re-sends.

---

## 11. Cursor policy

The driver **does not own or set the cursor**. glacier owns a single KMS hardware
cursor plane shared by every client (`DESIGN.md` §0.3, §3.2). Therefore:

- `WINEDRM_SetCursor` is a **no-op**.
- There is **no boot-cursor handshake** (winewayland's `set_boot_cursor` /
  `yetios_cursor_manager_v1` is gone — that whole saga is dead code under
  glacier).
- `WINEDRM_SetCursorPos` is, at most, a future warp *request* to the server; v1
  returns success without moving anything (the server is authoritative over the
  pointer).

This is a feature, not a gap: it is exactly the impedance mismatch the rewrite
exists to remove.

---

## 12. Display devices

`WINEDRM_UpdateDisplayDevices` reports a single monitor of `screen_w × screen_h`
taken from `CL_WELCOME`, positioned at `(0,0)` in the global virtual-screen
space. Resolution changes flow the other way — through glacier's control socket
to an atomic modeset (`DESIGN.md` §3.4, Phase 5) — not through this driver;
`ChangeDisplaySettings` support is a later addition once the protocol exposes a
mode-set request. Multi-output is a v2 extension (the protocol would gain an
output list in `CL_WELCOME`/a new message).

---

## 13. Build & integration

### 13.1 `Makefile.in`
```make
MODULE  = winedrm.drv
UNIXLIB = winedrm.so
UNIX_CFLAGS = $(XKBCOMMON_CFLAGS)              # + EGL/GBM when §8.4 lands
UNIX_LIBS   = -lwin32u $(XKBCOMMON_LIBS) $(PTHREAD_LIBS) -lm
IMPORTS = user32 win32u
SOURCES = \
    dllmain.c \
    display.c \
    event.c \
    keyboard.c \
    lattice.c \
    opengl.c \
    pointer.c \
    protocol.c \
    shmbuffer.c \
    surface.c \
    version.rc \
    vulkan.c \
    window.c \
    window_surface.c \
    winedrm_main.c
```
Note the absence of `wayland-client`, `wayland-egl`, and any `*.xml`/
`wayland-scanner` step. The CrystallineLattice transport is plain sockets +
the vendored `protocol.c`. (xkbcommon is kept for keysym constants/composition;
it can later be dropped via the static-XKB trick, `DESIGN.md` §4.)

### 13.2 configure.ac / build wiring
Add `dlls/winedrm.drv/Makefile` to `AC_CONFIG_FILES` (the Wine `make_makefiles`/
`configure` machinery). No new mandatory dependency for v1 beyond xkbcommon
(already required by winewayland.drv). EGL/GBM/libdrm become required only when
§8.4 is implemented; gate them behind a `--with-drm`-style check so v1 builds
without them.

### 13.3 Keeping the vendored protocol in sync
`protocol.h`/`protocol.c` are copied from glacier `include/protocol.h` +
`src/protocol.c`. Guard against drift:
- A header comment in both copies pointing at the glacier path + a `CL_VERSION`
  the copy was taken at.
- A build-time `C_ASSERT(CL_VERSION == <expected>)` so a version bump in glacier
  that isn't mirrored here fails the build loudly.
- (Optional) a `tools/sync-protocol.sh` that diffs the two and a CI check.

### 13.4 Default-driver registration
In `programs/explorer/desktop.c`, extend `default_driver` so `drm` is a candidate
(transition: `L"mac,x11,wayland,drm"`; glacier image: `L"drm"`). The desktop's
`GraphicsDriver` registry value can also pin it directly. While both
winewayland.drv and winedrm.drv exist, ordering decides which wins when both
backends are present; on a glacier session set it to `drm` only.

---

## 14. Dependencies

| Need | Source | When |
|---|---|---|
| sockets / `SCM_RIGHTS` | libc | v1 |
| `pthread` | libc | v1 (event thread, mutexes) |
| `win32u`, `user32` | Wine | v1 |
| xkbcommon | keysym constants + dead-key compose | v1 |
| libdrm, GBM, EGL, GLESv2 | dma-buf alloc + GL ICD | v2 (§8.4) |

Net vs. winewayland.drv: **drops** `wayland-client`, `wayland-egl`,
`wayland-protocols`, `wayland-scanner`; **keeps** xkbcommon; **adds nothing** for
v1. This matches the dependency-collapse thesis in `DESIGN.md` §4.

---

## 15. Versioning, reconnect, crash resilience

- **Handshake.** `CL_HELLO` advertises `CL_VERSION`/`CL_MIN_VERSION`; glacier
  replies `CL_WELCOME` with the negotiated version. Because it is a
  min-supported-version handshake (not strict equality), glacier and Moonshine
  can update as independent packages without a lockstep break (`DESIGN.md` §3.3).
  The driver must tolerate a negotiated version ≤ its own and refuse only below
  `CL_MIN_VERSION`.
- **Reconnect.** CrystallineLattice is designed so a client death is recoverable
  on the *server* side; symmetrically, `lattice_reconnect()` lets the driver
  re-establish the socket and re-issue `CL_CREATE_WINDOW`/`CL_SET_TITLE`/
  `CL_COMMIT` for every live `drm_win_data` if glacier restarts. v1 may ship the
  simpler "terminate process on link loss" behavior (matching winewayland) and
  add reconnect in M5/hardening (Phase 6).

---

## 16. Phased implementation plan (driver-local)

Maps onto `DESIGN.md` Phase 3 ("Transport A … winedrm.drv … first real Wine
client"). Each milestone is independently testable.

- **M1 — Connect & one window.** `lattice.c` + vendored `protocol.*` + `dllmain`/
  `winedrm_main` + minimal `window.c`/`window_surface.c`/`shmbuffer.c`. Goal:
  `notepad`/`winemine` opens one `CL_ROLE_NORMAL` window, contents committed as
  shm, placed by glacier with its server title bar. This is `glacier-client`
  parity from inside Wine. **Gate:** a Win32 app is visible in `glacier wm`.
- **M2 — Input.** `event.c` + `pointer.c` + `keyboard.c`. Route `CL_INPUT` to
  mouse + keyboard; keysym→VK + `WM_CHAR`. **Gate:** the app is interactive
  (type into notepad, click menus).
- **M3 — Lifecycle & multiple windows.** `CL_CONFIGURE`→`WindowPosChanged`,
  `CL_FOCUS`→activation, `CL_CLOSE`→`WM_CLOSE`; multiple HWNDs; roles for
  taskbar/desktop/menus. **Gate:** explorer shell chrome registers via roles (no
  heuristics); menus/tooltips behave.
- **M4 — dma-buf / GL.** §8.4: GBM allocation, dma-buf export, fence fd in
  `CL_COMMIT`, `OpenGLInit`/`VulkanInit` real paths. **Gate:** a GL/D3D app
  renders zero-copy with tear-free fencing.
- **M5 — Become the default.** Flip `default_driver`; add reconnect (§15);
  retire `winewayland.drv` from glacier images. **Gate:** `DESIGN.md` Phase 5
  parity — every frostedglass capability, none of the hacks.

---

## 17. Open questions to lock before/while coding

1. **Modifiers on the wire.** Keep deriving Shift/Ctrl/Alt from the keysym stream
   driver-side, or add a `mods` field to `cl_input` (v1 bump)? *Recommendation:
   add `mods`* — avoids server/driver modifier drift. (§9)
2. **Buffer release.** Does glacier add an explicit buffer-release event, or do we
   stay single-buffered until dma-buf brings explicit sync? Affects whether
   `shmbuffer.c` needs a pool in M1 or M4. (§8.3)
3. **Title encoding length.** `CL_TITLE_MAX` is 64 bytes; confirm UTF-8 truncation
   policy for long/multibyte titles. (§5)
4. **Interactive move/resize.** Does the shell own drag-move entirely (server
   draws the title bar, so likely yes), making `WINEDRM_SysCommand` a thin
   "request server drag" call, or is there a client role? (§5, §10)
5. **`SetCursorPos` semantics.** Pure no-op, or a real server warp request added
   to the protocol? Some games rely on it. (§11)
6. **Layered windows / per-pixel alpha.** Needs an alpha-capable format
   (`ARGB8888`) and a `CL_COMMIT` flag; defer to v2 or carry the flag now? (§5)

---

## Appendix — initial file checklist

Create as empty/stub first, fill per the §16 milestones:

- [ ] `Makefile.in`, `version.rc`, `winedrm_dll.h`, `unixlib.h`
- [ ] `protocol.h`, `protocol.c` (vendored, version-asserted)
- [ ] `dllmain.c`, `winedrm_main.c`, `lattice.h`, `lattice.c`
- [ ] `window.c`, `window_surface.c`, `surface.c`, `shmbuffer.c`
- [ ] `event.c`, `pointer.c`, `keyboard.c`, `display.c`
- [ ] `opengl.c` (stub), `vulkan.c` (stub)
- [ ] `programs/explorer/desktop.c` — add `drm` to `default_driver`
- [ ] `configure.ac` — register `dlls/winedrm.drv/Makefile`
