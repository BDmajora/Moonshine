/*
 * winedrm.drv — display device reporting
 *
 * Reports glacier's single global virtual screen (DESIGN.md §3.2) to win32u:
 * one GPU, one source, one monitor sized from CL_WELCOME at the origin.  Real
 * multi-output and mode lists are an M2+ extension once the protocol exports an
 * output list (SPEC §12).
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

UINT WINEDRM_UpdateDisplayDevices(const struct gdi_device_manager *device_manager, void *param)
{
    struct pci_id pci_id = {0};
    struct gdi_monitor monitor = {0};
    DEVMODEW mode = { .dmSize = sizeof(mode) };
    UINT dpi = NtUserGetSystemDpiForProcess(NULL);
    int w = process_lattice.screen_w;
    int h = process_lattice.screen_h;

    if (w <= 0 || h <= 0) { w = 1280; h = 1024; }

    TRACE("virtual screen %dx%d dpi %u\n", w, h, dpi);

    device_manager->add_gpu(NULL, &pci_id, NULL, param);
    device_manager->add_source("glacier0",
                               DISPLAY_DEVICE_ATTACHED_TO_DESKTOP | DISPLAY_DEVICE_PRIMARY_DEVICE,
                               dpi, param);

    SetRect(&monitor.rc_monitor, 0, 0, w, h);
    monitor.rc_work = monitor.rc_monitor;
    device_manager->add_monitor(&monitor, param);

    mode.dmFields = DM_DISPLAYORIENTATION | DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT |
                    DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY | DM_POSITION;
    mode.dmDisplayOrientation = DMDO_DEFAULT;
    mode.dmBitsPerPel = 32;
    mode.dmPelsWidth = w;
    mode.dmPelsHeight = h;
    mode.dmDisplayFrequency = 60;
    mode.dmPosition.x = 0;
    mode.dmPosition.y = 0;
    device_manager->add_modes(&mode, 1, &mode, param);

    return STATUS_SUCCESS;
}
