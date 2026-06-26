/*
 * winedrm.drv entry points (PE side)
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

#include "winedrm_dll.h"

#include "ntuser.h"
#include "winuser.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedrm);

static DWORD WINAPI winedrm_read_events_thread(void *arg)
{
    WINEDRM_UNIX_CALL(read_events, NULL);
    /* Returns only on an unrecoverable error (the link to glacier dropped). */
    ERR("Lost the connection to glacier, terminating process\n");
    TerminateProcess(GetCurrentProcess(), 1);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    DWORD tid;

    if (reason != DLL_PROCESS_ATTACH) return TRUE;

    DisableThreadLibraryCalls(instance);
    if (__wine_init_unix_call()) return FALSE;

    /* Fails the load (and thus falls back to the next graphics driver) when
     * glacier is not reachable.  No boot-cursor handshake and no clipboard
     * thread: glacier owns the cursor (KMS plane) and clipboard is post-M1. */
    if (WINEDRM_UNIX_CALL(init, NULL))
        return FALSE;

    /* Read glacier events from a dedicated thread. */
    CloseHandle(CreateThread(NULL, 0, winedrm_read_events_thread, NULL, 0, &tid));

    return TRUE;
}
