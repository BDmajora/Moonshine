/*
 * winedrm.drv initialization — unixlib entry table + USER driver vtable
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

/* The vendored protocol must stay in lockstep with glacier's copy; a server
 * version bump that isn't mirrored here should fail the build, not silently
 * mis-handshake.  See SPEC.md §13.3. */
C_ASSERT(CL_VERSION == 1u);

char *process_name = NULL;

static const struct user_driver_funcs winedrm_funcs =
{
    .pUpdateDisplayDevices = WINEDRM_UpdateDisplayDevices,
    .pCreateWindowSurface  = WINEDRM_CreateWindowSurface,
    .pDestroyWindow        = WINEDRM_DestroyWindow,
    .pSetWindowText        = WINEDRM_SetWindowText,
    .pWindowMessage        = WINEDRM_WindowMessage,
    .pWindowPosChanging    = WINEDRM_WindowPosChanging,
    .pWindowPosChanged     = WINEDRM_WindowPosChanged,
    .pDesktopWindowProc    = WINEDRM_DesktopWindowProc,
    /* Cursor/keyboard/GL fall back to win32u's nulldrv stubs for M1.  Cursor
     * stays a no-op by design: glacier owns the KMS cursor plane (SPEC §11). */
};

static void winedrm_init_process_name(void)
{
    WCHAR *p, *appname;
    WCHAR appname_lower[MAX_PATH];
    DWORD appname_len, appnamez_size, utf8_size;
    int i;

    appname = NtCurrentTeb()->Peb->ProcessParameters->ImagePathName.Buffer;
    if ((p = wcsrchr(appname, '/'))) appname = p + 1;
    if ((p = wcsrchr(appname, '\\'))) appname = p + 1;
    appname_len = lstrlenW(appname);

    if (appname_len == 0 || appname_len >= MAX_PATH) return;

    for (i = 0; appname[i]; i++) appname_lower[i] = RtlDowncaseUnicodeChar(appname[i]);
    appname_lower[i] = 0;

    appnamez_size = (appname_len + 1) * sizeof(WCHAR);

    if (!RtlUnicodeToUTF8N(NULL, 0, &utf8_size, appname_lower, appnamez_size) &&
        (process_name = malloc(utf8_size)))
    {
        RtlUnicodeToUTF8N(process_name, utf8_size, &utf8_size, appname_lower, appnamez_size);
    }
}

static NTSTATUS winedrm_unix_init(void *arg)
{
    /* Publish the driver vtable up front so it is available during init.  We
     * clear it again on error so Wine can fall through to the next driver. */
    __wine_set_user_driver(&winedrm_funcs, WINE_GDI_DRIVER_VERSION);

    winedrm_init_process_name();

    if (!lattice_process_init()) goto err;

    return STATUS_SUCCESS;

err:
    __wine_set_user_driver(NULL, WINE_GDI_DRIVER_VERSION);
    return STATUS_UNSUCCESSFUL;
}

static NTSTATUS winedrm_unix_read_events(void *arg)
{
    winedrm_dispatch_events();
    /* Only returns on a fatal error (e.g. the link to glacier dropped). */
    return STATUS_UNSUCCESSFUL;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    winedrm_unix_init,
    winedrm_unix_read_events,
};

C_ASSERT(ARRAYSIZE(__wine_unix_call_funcs) == winedrm_unix_func_count);

#ifdef _WIN64

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    winedrm_unix_init,
    winedrm_unix_read_events,
};

C_ASSERT(ARRAYSIZE(__wine_unix_call_wow64_funcs) == winedrm_unix_func_count);

#endif /* _WIN64 */
