/*
 * Copyright 2024 Rémi Bernon for CodeWeavers
 * Copyright 2025 YetiOS contributors
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

#include <stddef.h>
#include <stdarg.h>

#include <windef.h>
#include <winbase.h>
#include <winuser.h>
#include <commctrl.h>

/* strings */
#define IDS_CPL_NAME        1
#define IDS_CPL_INFO        2

/* dialogs */
#define IDD_DESKTOP         1000    /* main display page                    */
#define IDD_ADAPTER         1001    /* Advanced → Adapter tab               */
#define IDD_MONITOR         1002    /* Advanced → Monitor tab               */

/* main dialog controls */
#define IDC_STATIC                  -1
#define IDC_VIRTUAL_DESKTOP         2000
#define IDC_OUTPUT_COMBO            2001
#define IDC_RESOLUTION_COMBO        2002
#define IDC_ORIENTATION_COMBO       2003
#define IDC_ADVANCED_BUTTON         2004

/* adapter tab controls */
#define IDC_ADAPTER_TYPE            2100
#define IDC_ADAPTER_INFO            2101

/* monitor tab controls */
#define IDC_MONITOR_TYPE            2200
#define IDC_REFRESH_COMBO           2201
#define IDC_VRR_CHECK               2202

/* icon */
#define ICO_MAIN            100