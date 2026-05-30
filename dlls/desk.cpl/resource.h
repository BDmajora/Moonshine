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
#define IDD_TROUBLESHOOT    1003    /* Advanced → Troubleshoot tab          */
#define IDD_COLORMGMT       1004    /* Advanced → Color Management tab      */
#define IDD_LISTMODES       1005    /* Adapter → List All Modes dialog      */

/* main dialog controls */
#define IDC_STATIC                  -1
#define IDC_VIRTUAL_DESKTOP         2000
#define IDC_OUTPUT_COMBO            2001
#define IDC_RESOLUTION_COMBO        2002
#define IDC_ORIENTATION_COMBO       2003
#define IDC_ADVANCED_BUTTON         2004
#define IDC_DETECT_BUTTON           2005
#define IDC_IDENTIFY_BUTTON         2006

/* adapter tab controls */
#define IDC_ADAPTER_TYPE            2100
#define IDC_ADAPTER_INFO            2101
#define IDC_ADAPTER_ICON            2102
#define IDC_ADAPTER_PROPS           2103
#define IDC_LIST_MODES              2104

/* monitor tab controls */
#define IDC_MONITOR_TYPE            2200
#define IDC_REFRESH_COMBO           2201
#define IDC_VRR_CHECK               2202
#define IDC_MONITOR_PROPS           2203
#define IDC_HIDE_MODES_CHECK        2204
#define IDC_COLORS_COMBO            2205

/* troubleshoot tab controls */
#define IDC_HWACCEL_TRACK           2300
#define IDC_CHANGE_SETTINGS         2301

/* color management tab controls */
#define IDC_PROFILE_LIST            2400
#define IDC_PROFILE_ADD             2401
#define IDC_PROFILE_REMOVE          2402
#define IDC_PROFILE_DEFAULT         2403
#define IDC_COLORMGMT_LAUNCH        2404

/* list all modes controls */
#define IDC_MODES_LIST              2500

/* icon */
#define ICO_MAIN            100