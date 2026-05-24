/*
 * desktop_defs.h — Shared definitions for the YetiOS desktop root window.
 *
 * desktop.exe is the bottom-most window of the YetiOS Wine shell.  It is
 * a full-screen, borderless WS_POPUP that paints the wallpaper and gives
 * the frostedglass compositor a real Wine surface to sit at the bottom of
 * the scene graph.  Because a Wine surface always covers the screen, the
 * pointer is always over a Wine window, so Wine always sets its own
 * cursor — the Linux/wlroots cursor is never shown.
 *
 * The window class name below is the contract between this program and
 * the compositor: frostedglass keys on it (via app_id / title) to detect
 * the desktop window and lower it to the bottom of the scene graph.
 */

#ifndef DESKTOP_DEFS_H
#define DESKTOP_DEFS_H

#include <windows.h>

/* ------------------------------------------------------------------ */
/* Compositor contract                                                */
/* ------------------------------------------------------------------ */

/*
 * Window class and title.  frostedglass uses these to identify the
 * desktop window.  Keep them stable — changing them means changing the
 * detection heuristic on the compositor side too.
 */
#define DESKTOP_WND_CLASS  "YetiOSDesktop"
#define DESKTOP_WND_TITLE  "YetiOS Desktop"

/* ------------------------------------------------------------------ */
/* Appearance                                                         */
/* ------------------------------------------------------------------ */

/*
 * Hard-coded wallpaper color for now: a light purple.
 * Later this will be read from the registry (theme settings).
 */
#define DESKTOP_BG_R  200
#define DESKTOP_BG_G  170
#define DESKTOP_BG_B  235

/* ------------------------------------------------------------------ */
/* Resource IDs                                                       */
/* ------------------------------------------------------------------ */

#define IDI_DESKTOP  1

/* ------------------------------------------------------------------ */
/* Window procedure                                                   */
/* ------------------------------------------------------------------ */

LRESULT CALLBACK desktop_wndproc(HWND hwnd, UINT msg,
    WPARAM wparam, LPARAM lparam);

#endif /* DESKTOP_DEFS_H */