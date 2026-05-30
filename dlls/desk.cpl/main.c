/*
 * desk.cpl — Display Settings control-panel applet.
 *
 * Mirrors the Windows 7/8 "Screen Resolution" panel layout:
 *   Main page  — monitor preview, Display / Resolution / Orientation combos
 *   Advanced   — tabbed property sheet with Adapter + Monitor pages
 *                (refresh-rate combo, VRR checkbox on the Monitor page)
 *
 * All hardware interaction goes through wlr-randr via the companion
 * Unix library (unixlib.c / desk.so).
 *
 * Copyright 2024 Rémi Bernon for CodeWeavers
 * Copyright 2025 YetiOS contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "desk_private.h"

#include <commctrl.h>
#include <cpl.h>
#include "ole2.h"
#include "shellapi.h"

#include "wine/debug.h"
#include "wine/unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(deskcpl);

static HMODULE module;

/* ================================================================== */
/* Unix-call helpers                                                  */
/* ================================================================== */

#define UNIX_CALL(code, params) \
    __wine_unix_call(__wine_unixlib_handle, (code), (params))

static BOOL unix_inited;

static BOOL ensure_unix(void)
{
    if (!unix_inited)
    {
        if (__wine_init_unix_call())
        {
            ERR("Failed to initialise desk.cpl unix library.\n");
            return FALSE;
        }
        unix_inited = TRUE;
    }
    return TRUE;
}

/* ================================================================== */
/* Global state — cached wlr-randr data + user selections             */
/* ================================================================== */

static struct wlr_enumerate_params wlr_data;
static int            sel_output;          /* index or -1               */
static unsigned int   staged_w, staged_h;  /* resolution to apply       */
static unsigned int   staged_transform;    /* orientation to apply       */

static const struct wlr_output_info *get_output(void)
{
    if (sel_output < 0 || sel_output >= (int)wlr_data.num_outputs)
        return NULL;
    return &wlr_data.outputs[sel_output];
}

static BOOL enumerate_outputs(void)
{
    if (!ensure_unix()) return FALSE;
    return UNIX_CALL(unix_wlr_enumerate, &wlr_data) == 0;
}

/* Current desktop color depth, in bits per pixel. */
static int current_bpp(void)
{
    HDC hdc = GetDC(NULL);
    int bpp = GetDeviceCaps(hdc, BITSPIXEL);
    ReleaseDC(NULL, hdc);
    return bpp;
}

/* ================================================================== */
/* Orientation helpers                                                */
/* ================================================================== */

static const WCHAR *orientation_labels[] =
{
    L"Landscape",
    L"Portrait",
    L"Landscape (flipped)",
    L"Portrait (flipped)",
};

/* ================================================================== */
/* Main dialog — combo population                                     */
/* ================================================================== */

static void populate_output_combo(HWND hwnd)
{
    HWND combo = GetDlgItem(hwnd, IDC_OUTPUT_COMBO);
    unsigned int i;
    int first_enabled = -1;

    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    sel_output = -1;

    for (i = 0; i < wlr_data.num_outputs; i++)
    {
        const struct wlr_output_info *out = &wlr_data.outputs[i];
        WCHAR buf[384], wname[64], wdesc[256];

        MultiByteToWideChar(CP_UTF8, 0, out->name, -1, wname, 64);

        if (out->description[0])
        {
            MultiByteToWideChar(CP_UTF8, 0, out->description, -1, wdesc, 256);
            swprintf(buf, ARRAY_SIZE(buf), L"%u. %s", i + 1, wdesc);
        }
        else
            swprintf(buf, ARRAY_SIZE(buf), L"%u. %s", i + 1, wname);

        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)buf);
        SendMessageW(combo, CB_SETITEMDATA, i, (LPARAM)i);

        if (first_enabled < 0 && out->enabled)
            first_enabled = (int)i;
    }

    if (first_enabled < 0 && wlr_data.num_outputs > 0)
        first_enabled = 0;

    if (first_enabled >= 0)
    {
        SendMessageW(combo, CB_SETCURSEL, first_enabled, 0);
        sel_output = first_enabled;
    }
}

static void populate_resolution_combo(HWND hwnd)
{
    HWND combo = GetDlgItem(hwnd, IDC_RESOLUTION_COMBO);
    const struct wlr_output_info *out = get_output();
    unsigned int pairs[MAX_WLR_MODES][2];
    int          is_preferred[MAX_WLR_MODES];
    unsigned int unique = 0, i, j;
    int cur_sel = -1;

    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    staged_w = staged_h = 0;

    if (!out) return;

    /* Collect unique WxH, preserving wlr-randr order (largest first). */
    for (i = 0; i < out->num_modes; i++)
    {
        unsigned int w = out->modes[i].width, h = out->modes[i].height;
        BOOL dup = FALSE;

        for (j = 0; j < unique; j++)
            if (pairs[j][0] == w && pairs[j][1] == h) { dup = TRUE; break; }

        if (!dup && unique < MAX_WLR_MODES)
        {
            pairs[unique][0] = w;
            pairs[unique][1] = h;
            is_preferred[unique] = 0;
            unique++;
        }
    }

    /* Mark preferred. */
    for (i = 0; i < out->num_modes; i++)
    {
        if (!out->modes[i].preferred) continue;
        for (j = 0; j < unique; j++)
            if (pairs[j][0] == out->modes[i].width &&
                pairs[j][1] == out->modes[i].height)
                is_preferred[j] = 1;
    }

    /* Populate. */
    for (i = 0; i < unique; i++)
    {
        WCHAR buf[80];
        if (is_preferred[i])
            swprintf(buf, ARRAY_SIZE(buf),
                     L"%u \u00d7 %u (Recommended)", pairs[i][0], pairs[i][1]);
        else
            swprintf(buf, ARRAY_SIZE(buf),
                     L"%u \u00d7 %u", pairs[i][0], pairs[i][1]);

        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)buf);

        /* Is this the current resolution? */
        for (j = 0; j < out->num_modes; j++)
        {
            if (out->modes[j].current &&
                out->modes[j].width  == pairs[i][0] &&
                out->modes[j].height == pairs[i][1])
                cur_sel = (int)i;
        }
    }

    if (cur_sel < 0 && unique > 0) cur_sel = 0;
    if (cur_sel >= 0)
    {
        SendMessageW(combo, CB_SETCURSEL, cur_sel, 0);
        staged_w = pairs[cur_sel][0];
        staged_h = pairs[cur_sel][1];
    }
}

static void populate_orientation_combo(HWND hwnd)
{
    HWND combo = GetDlgItem(hwnd, IDC_ORIENTATION_COMBO);
    const struct wlr_output_info *out = get_output();
    unsigned int i;

    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    staged_transform = WLR_TRANSFORM_NORMAL;

    for (i = 0; i < 4; i++)
    {
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)orientation_labels[i]);
        SendMessageW(combo, CB_SETITEMDATA, i, (LPARAM)i);
    }

    if (out) staged_transform = out->transform;
    SendMessageW(combo, CB_SETCURSEL, staged_transform, 0);
}

/* Read back the resolution from the combo (parses "W × H …"). */
static void read_resolution_combo(HWND hwnd)
{
    int idx = (int)SendDlgItemMessageW(hwnd, IDC_RESOLUTION_COMBO,
                                       CB_GETCURSEL, 0, 0);
    WCHAR buf[80];
    unsigned int w = 0, h = 0;

    if (idx < 0) return;
    SendDlgItemMessageW(hwnd, IDC_RESOLUTION_COMBO,
                        CB_GETLBTEXT, idx, (LPARAM)buf);

    /* Parse "1920 × 1080" — the × is U+00D7 but we just skip non-digits. */
    if (swscanf(buf, L"%u %*c %u", &w, &h) >= 2 ||
        swscanf(buf, L"%u%*c%u", &w, &h) >= 2)
    {
        staged_w = w;
        staged_h = h;
    }
}

/* ================================================================== */
/* Main dialog — apply                                                */
/* ================================================================== */

static BOOL apply_main_settings(HWND hwnd)
{
    const struct wlr_output_info *out = get_output();
    struct wlr_apply_params ap;

    if (!out || !ensure_unix()) return FALSE;

    memset(&ap, 0, sizeof(ap));
    lstrcpynA(ap.output_name, out->name, sizeof(ap.output_name));
    ap.flags     = WLR_APPLY_MODE | WLR_APPLY_TRANSFORM;
    ap.width     = staged_w;
    ap.height    = staged_h;
    ap.transform = staged_transform;

    TRACE("Main apply: %ux%u transform=%u on %s\n",
          staged_w, staged_h, staged_transform, out->name);

    if (UNIX_CALL(unix_wlr_apply, &ap) != 0)
    {
        MessageBoxW(hwnd, L"Failed to apply display settings.\n\n"
                    L"The selected mode may not be supported by your display.",
                    L"Display Settings", MB_ICONWARNING | MB_OK);
        return FALSE;
    }

    /* Re-enumerate so everything is consistent. */
    enumerate_outputs();
    populate_output_combo(hwnd);
    populate_resolution_combo(hwnd);
    populate_orientation_combo(hwnd);
    InvalidateRect(GetDlgItem(hwnd, IDC_VIRTUAL_DESKTOP), NULL, TRUE);
    return TRUE;
}

/* ================================================================== */
/* Monitor-layout preview                                             */
/* ================================================================== */

static RECT compute_bounding(void)
{
    RECT b = {0};
    unsigned int i;

    for (i = 0; i < wlr_data.num_outputs; i++)
    {
        const struct wlr_output_info *o = &wlr_data.outputs[i];
        unsigned int w = 0, h = 0, m;
        RECT r;

        if (!o->enabled) continue;

        for (m = 0; m < o->num_modes; m++)
            if (o->modes[m].current) { w = o->modes[m].width; h = o->modes[m].height; break; }
        if (!w && o->num_modes) { w = o->modes[0].width; h = o->modes[0].height; }
        if (!w) { w = 1920; h = 1080; }

        SetRect(&r, o->pos_x, o->pos_y, o->pos_x + (int)w, o->pos_y + (int)h);
        if (i == 0) b = r;
        else        UnionRect(&b, &b, &r);
    }
    if (b.right <= b.left) { b.right = 1920; b.bottom = 1080; }
    return b;
}

static void draw_monitor(HDC hdc, const struct wlr_output_info *o,
                         RECT client, RECT virt, float scale, BOOL is_sel,
                         unsigned int index)
{
    unsigned int w = 0, h = 0, m;
    RECT src, dst;
    WCHAR label[32];

    for (m = 0; m < o->num_modes; m++)
        if (o->modes[m].current) { w = o->modes[m].width; h = o->modes[m].height; break; }
    if (!w && o->num_modes) { w = o->modes[0].width; h = o->modes[0].height; }
    if (!w) { w = 1920; h = 1080; }

    SetRect(&src, o->pos_x, o->pos_y, o->pos_x + (int)w, o->pos_y + (int)h);

    /* Map virtual → client coordinates. */
    OffsetRect(&src, -(virt.left + virt.right) / 2, -(virt.top + virt.bottom) / 2);
    dst.left   = (LONG)(src.left   * scale) + (client.left + client.right)  / 2;
    dst.top    = (LONG)(src.top    * scale) + (client.top  + client.bottom) / 2;
    dst.right  = (LONG)(src.right  * scale) + (client.left + client.right)  / 2;
    dst.bottom = (LONG)(src.bottom * scale) + (client.top  + client.bottom) / 2;

    /* Fill. */
    SelectObject(hdc, GetStockObject(DC_BRUSH));
    SelectObject(hdc, GetStockObject(DC_PEN));
    SetDCBrushColor(hdc, GetSysColor(is_sel ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    SetDCPenColor(hdc, GetSysColor(COLOR_WINDOWFRAME));
    Rectangle(hdc, dst.left, dst.top, dst.right, dst.bottom);

    /* Number label (1-based like Windows). */
    swprintf(label, ARRAY_SIZE(label), L"%u", index + 1);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(is_sel ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    {
        HFONT big = CreateFontW(-28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                DEFAULT_PITCH, L"Ms Shell Dlg");
        HFONT old = SelectObject(hdc, big);
        DrawTextW(hdc, label, -1, &dst, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
        SelectObject(hdc, old);
        DeleteObject(big);
    }
}

static LRESULT CALLBACK desktop_view_proc(HWND hwnd, UINT msg,
                                          WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        RECT client, virt;
        float scale;
        unsigned int i;
        HDC hdc;

        GetClientRect(hwnd, &client);
        hdc = BeginPaint(hwnd, &ps);

        /* Dark-blue gradient background (simplified: solid). */
        {
            HBRUSH bg = CreateSolidBrush(RGB(58, 110, 165));
            FillRect(hdc, &client, bg);
            DeleteObject(bg);
        }

        virt  = compute_bounding();
        scale = min((float)(client.right  - client.left) / (float)(virt.right  - virt.left),
                    (float)(client.bottom - client.top)  / (float)(virt.bottom - virt.top));
        scale *= 0.80f;

        for (i = 0; i < wlr_data.num_outputs; i++)
        {
            if (!wlr_data.outputs[i].enabled) continue;
            draw_monitor(hdc, &wlr_data.outputs[i], client, virt, scale,
                         (int)i == sel_output, i);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    if (msg == WM_LBUTTONDOWN)
    {
        /* TODO: hit-test for multi-monitor selection. */
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static void create_desktop_view(HWND hwnd)
{
    HWND parent = GetDlgItem(hwnd, IDC_VIRTUAL_DESKTOP);
    RECT rc;
    LONG m;

    GetClientRect(parent, &rc);
    rc.top += 4;
    m = (rc.bottom - rc.top) * 4 / 100;
    InflateRect(&rc, -m, -m);

    {
        HWND view = CreateWindowW(L"DeskCplDesktop", NULL, WS_CHILD,
                                  rc.left, rc.top,
                                  rc.right - rc.left, rc.bottom - rc.top,
                                  parent, NULL, NULL, module);
        SetWindowLongPtrW(view, GWLP_USERDATA, (UINT_PTR)hwnd);
        ShowWindow(view, SW_SHOW);
    }
}

/* ================================================================== */
/* List All Modes dialog                                              */
/* ================================================================== */

static INT_PTR CALLBACK list_modes_dialog_proc(HWND hwnd, UINT msg,
                                               WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        const struct wlr_output_info *out = get_output();
        HWND lb = GetDlgItem(hwnd, IDC_MODES_LIST);
        int bpp = current_bpp();
        const WCHAR *colors = (bpp >= 32) ? L"True Color (32 bit)" :
                              (bpp >= 16) ? L"High Color (16 bit)" :
                                            L"256 Colors (8 bit)";
        unsigned int i;

        if (!out) return TRUE;

        /* One row per supported resolution + refresh combination. */
        for (i = 0; i < out->num_modes; i++)
        {
            WCHAR buf[96];
            unsigned int hz = out->modes[i].refresh_mhz / 1000;
            int idx;

            swprintf(buf, ARRAY_SIZE(buf), L"%u \u00d7 %u, %s, %u Hertz",
                     out->modes[i].width, out->modes[i].height, colors, hz);
            idx = (int)SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)buf);
            SendMessageW(lb, LB_SETITEMDATA, idx, (LPARAM)i);
            if (out->modes[i].current)
                SendMessageW(lb, LB_SETCURSEL, idx, 0);
        }
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wparam) == IDOK)
        {
            const struct wlr_output_info *out = get_output();
            HWND lb = GetDlgItem(hwnd, IDC_MODES_LIST);
            int sel = (int)SendMessageW(lb, LB_GETCURSEL, 0, 0);

            if (out && sel >= 0 && ensure_unix())
            {
                unsigned int mi = (unsigned int)SendMessageW(lb, LB_GETITEMDATA, sel, 0);
                struct wlr_apply_params ap;

                memset(&ap, 0, sizeof(ap));
                lstrcpynA(ap.output_name, out->name, sizeof(ap.output_name));
                ap.flags       = WLR_APPLY_MODE | WLR_APPLY_REFRESH;
                ap.width       = out->modes[mi].width;
                ap.height      = out->modes[mi].height;
                ap.refresh_mhz = out->modes[mi].refresh_mhz;
                if (UNIX_CALL(unix_wlr_apply, &ap) == 0)
                    enumerate_outputs();
            }
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        if (LOWORD(wparam) == IDCANCEL)
        {
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/* ================================================================== */
/* Advanced Settings — Adapter tab                                    */
/* ================================================================== */

static INT_PTR CALLBACK adapter_dialog_proc(HWND hwnd, UINT msg,
                                            WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        const struct wlr_output_info *out = get_output();
        WCHAR info[640];

        SetDlgItemTextW(hwnd, IDC_ADAPTER_TYPE,
                        out ? L"frostedglass (wlroots)" : L"Unknown");

        if (out)
            swprintf(info, ARRAY_SIZE(info),
                     L"Chip Type:  frostedglass virtual GPU\r\n"
                     L"DAC Type:  Integrated\r\n"
                     L"Adapter String:  frostedglass (wlroots)\r\n"
                     L"BIOS Information:  frostedglass VBIOS\r\n"
                     L"\r\n"
                     L"Total Available Graphics Memory:  %u MB\r\n"
                     L"Dedicated Video Memory:  %u MB\r\n"
                     L"System Video Memory:  %u MB\r\n"
                     L"Shared System Memory:  %u MB",
                     4096u, 1024u, 0u, 3072u);
        else
            swprintf(info, ARRAY_SIZE(info), L"No display selected.");

        SetDlgItemTextW(hwnd, IDC_ADAPTER_INFO, info);
        return TRUE;
    }

    case WM_COMMAND:
        /* Properties — open the device entry in Device Manager. */
        if (LOWORD(wparam) == IDC_ADAPTER_PROPS)
            ShellExecuteW(hwnd, L"open", L"devmgmt.msc", NULL, NULL, SW_SHOWNORMAL);
        /* List All Modes — modal list of every supported mode. */
        else if (LOWORD(wparam) == IDC_LIST_MODES)
            DialogBoxW(module, MAKEINTRESOURCEW(IDD_LISTMODES), hwnd,
                       list_modes_dialog_proc);
        return TRUE;
    }
    return FALSE;
}

/* ================================================================== */
/* Advanced Settings — Monitor tab (refresh rate + VRR)               */
/* ================================================================== */

/* State for the monitor dialog — initialised on WM_INITDIALOG. */
static unsigned int mon_rates[MAX_WLR_MODES];
static unsigned int mon_rate_count;

static INT_PTR CALLBACK monitor_dialog_proc(HWND hwnd, UINT msg,
                                            WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        const struct wlr_output_info *out = get_output();
        HWND combo = GetDlgItem(hwnd, IDC_REFRESH_COMBO);
        unsigned int i, j;
        int cur_sel = -1;

        /* Re-read live state so we reflect anything the main dialog just applied. */
        enumerate_outputs();

        out = get_output();
        mon_rate_count = 0;

        if (!out) { SetDlgItemTextW(hwnd, IDC_MONITOR_TYPE, L"No display"); return TRUE; }

        /* Monitor name. */
        {
            WCHAR wdesc[256];
            MultiByteToWideChar(CP_UTF8, 0, out->description[0] ? out->description : out->name,
                                -1, wdesc, 256);
            SetDlgItemTextW(hwnd, IDC_MONITOR_TYPE, wdesc);
        }

        /* Determine current resolution (what's actually live). */
        {
            unsigned int cur_w = 0, cur_h = 0, cur_mhz = 0;
            for (i = 0; i < out->num_modes; i++)
            {
                if (out->modes[i].current)
                {
                    cur_w   = out->modes[i].width;
                    cur_h   = out->modes[i].height;
                    cur_mhz = out->modes[i].refresh_mhz;
                    break;
                }
            }

            /* Collect refresh rates for the current resolution. */
            for (i = 0; i < out->num_modes; i++)
            {
                if (out->modes[i].width != cur_w || out->modes[i].height != cur_h)
                    continue;
                if (mon_rate_count < MAX_WLR_MODES)
                    mon_rates[mon_rate_count++] = out->modes[i].refresh_mhz;
            }

            /* Sort descending. */
            for (i = 0; i < mon_rate_count; i++)
                for (j = i + 1; j < mon_rate_count; j++)
                    if (mon_rates[j] > mon_rates[i])
                    {
                        unsigned int t = mon_rates[i];
                        mon_rates[i] = mon_rates[j];
                        mon_rates[j] = t;
                    }

            /* Populate combo. */
            SendMessageW(combo, CB_RESETCONTENT, 0, 0);
            for (i = 0; i < mon_rate_count; i++)
            {
                WCHAR buf[64];
                unsigned int hz_int = mon_rates[i] / 1000;
                swprintf(buf, ARRAY_SIZE(buf), L"%u Hertz", hz_int);
                SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)buf);
                SendMessageW(combo, CB_SETITEMDATA, i, (LPARAM)mon_rates[i]);

                if (mon_rates[i] == cur_mhz)
                    cur_sel = (int)i;
            }

            if (cur_sel < 0 && mon_rate_count > 0) cur_sel = 0;
            if (cur_sel >= 0) SendMessageW(combo, CB_SETCURSEL, cur_sel, 0);
        }

        /* VRR checkbox. */
        SendDlgItemMessageW(hwnd, IDC_VRR_CHECK, BM_SETCHECK,
                            out->adaptive_sync ? BST_CHECKED : BST_UNCHECKED, 0);

        /* Hide-modes checkbox — checked by default, matches Windows. */
        SendDlgItemMessageW(hwnd, IDC_HIDE_MODES_CHECK, BM_SETCHECK, BST_CHECKED, 0);

        /* Colors combo — bit-depth options, selected from the live depth. */
        {
            HWND cc = GetDlgItem(hwnd, IDC_COLORS_COMBO);
            int bpp = current_bpp();
            SendMessageW(cc, CB_RESETCONTENT, 0, 0);
            SendMessageW(cc, CB_ADDSTRING, 0, (LPARAM)L"High Color (16 bit)");
            SendMessageW(cc, CB_ADDSTRING, 0, (LPARAM)L"True Color (32 bit)");
            SendMessageW(cc, CB_SETCURSEL, (bpp >= 32) ? 1 : 0, 0);
        }

        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wparam) == IDC_REFRESH_COMBO && HIWORD(wparam) == CBN_SELCHANGE)
            SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
        if (LOWORD(wparam) == IDC_VRR_CHECK)
            SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
        if (LOWORD(wparam) == IDC_COLORS_COMBO && HIWORD(wparam) == CBN_SELCHANGE)
            SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
        if (LOWORD(wparam) == IDC_HIDE_MODES_CHECK)
            SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
        /* Properties — open the monitor entry in Device Manager. */
        if (LOWORD(wparam) == IDC_MONITOR_PROPS)
            ShellExecuteW(hwnd, L"open", L"devmgmt.msc", NULL, NULL, SW_SHOWNORMAL);
        return TRUE;

    case WM_NOTIFY:
    {
        NMHDR *nm = (NMHDR *)lparam;
        if (nm->code == PSN_APPLY)
        {
            const struct wlr_output_info *out = get_output();
            struct wlr_apply_params ap;
            int idx;

            if (!out || !ensure_unix())
            {
                SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, PSNRET_INVALID);
                return TRUE;
            }

            memset(&ap, 0, sizeof(ap));
            lstrcpynA(ap.output_name, out->name, sizeof(ap.output_name));

            /* Refresh rate. */
            idx = (int)SendDlgItemMessageW(hwnd, IDC_REFRESH_COMBO,
                                           CB_GETCURSEL, 0, 0);
            if (idx >= 0)
            {
                ap.refresh_mhz = (unsigned int)SendDlgItemMessageW(
                    hwnd, IDC_REFRESH_COMBO, CB_GETITEMDATA, idx, 0);
                ap.flags |= WLR_APPLY_REFRESH;
            }

            /* VRR. */
            ap.adaptive_sync = (IsDlgButtonChecked(hwnd, IDC_VRR_CHECK) == BST_CHECKED);
            ap.flags |= WLR_APPLY_VRR;

            TRACE("Monitor apply: refresh=%u mHz vrr=%d on %s\n",
                  ap.refresh_mhz, ap.adaptive_sync, out->name);

            if (UNIX_CALL(unix_wlr_apply, &ap) != 0)
            {
                MessageBoxW(hwnd,
                    L"Failed to apply monitor settings.\n\n"
                    L"The selected refresh rate may not be supported, "
                    L"or VRR may not be available for this display.",
                    L"Monitor Settings", MB_ICONWARNING | MB_OK);
                SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, PSNRET_INVALID);
                return TRUE;
            }

            /* Colors — apply bit depth via GDI, with a confirmation. */
            {
                int csel = (int)SendDlgItemMessageW(hwnd, IDC_COLORS_COMBO,
                                                    CB_GETCURSEL, 0, 0);
                int want = (csel == 1) ? 32 : (csel == 0) ? 16 : 0;
                if (want && want != current_bpp())
                {
                    if (MessageBoxW(hwnd, L"Apply the new color settings?",
                                    L"Monitor Settings",
                                    MB_YESNO | MB_ICONQUESTION) == IDYES)
                    {
                        DEVMODEW dm;
                        memset(&dm, 0, sizeof(dm));
                        dm.dmSize       = sizeof(dm);
                        dm.dmFields     = DM_BITSPERPEL;
                        dm.dmBitsPerPel = want;
                        ChangeDisplaySettingsExW(NULL, &dm, NULL, 0, NULL);
                    }
                }
            }

            SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        break;
    }
    }

    return FALSE;
}

/* ================================================================== */
/* Advanced Settings — Troubleshoot tab                               */
/* ================================================================== */

static INT_PTR CALLBACK troubleshoot_dialog_proc(HWND hwnd, UINT msg,
                                                 WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        HWND tb = GetDlgItem(hwnd, IDC_HWACCEL_TRACK);
        /* Acceleration slider — None (0) .. Full (5), default Full. */
        SendMessageW(tb, TBM_SETRANGE, TRUE, MAKELONG(0, 5));
        SendMessageW(tb, TBM_SETPOS, TRUE, 5);
        return TRUE;
    }

    case WM_COMMAND:
        /* Change settings — launch DirectX diagnostics. */
        if (LOWORD(wparam) == IDC_CHANGE_SETTINGS)
            ShellExecuteW(hwnd, L"open", L"dxdiag.exe", NULL, NULL, SW_SHOWNORMAL);
        return TRUE;
    }
    return FALSE;
}

/* ================================================================== */
/* Advanced Settings — Color Management tab                           */
/* ================================================================== */

static INT_PTR CALLBACK color_mgmt_dialog_proc(HWND hwnd, UINT msg,
                                               WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        SendDlgItemMessageW(hwnd, IDC_PROFILE_LIST, LB_ADDSTRING, 0,
                            (LPARAM)L"sRGB IEC61966-2.1 (default)");
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        /* Add / full applet — open the Color Management control panel. */
        case IDC_PROFILE_ADD:
        case IDC_COLORMGMT_LAUNCH:
            ShellExecuteW(hwnd, L"open", L"colorcpl.exe", NULL, NULL, SW_SHOWNORMAL);
            break;

        case IDC_PROFILE_REMOVE:
        {
            HWND lb = GetDlgItem(hwnd, IDC_PROFILE_LIST);
            int sel = (int)SendMessageW(lb, LB_GETCURSEL, 0, 0);
            if (sel >= 0) SendMessageW(lb, LB_DELETESTRING, sel, 0);
            break;
        }

        case IDC_PROFILE_DEFAULT:
            MessageBoxW(hwnd,
                        L"Set the selected profile as the default for this device.",
                        L"Color Management", MB_OK | MB_ICONINFORMATION);
            break;
        }
        return TRUE;
    }
    return FALSE;
}

/* ================================================================== */
/* Open Advanced Settings property sheet                               */
/* ================================================================== */

static void open_advanced_settings(HWND parent)
{
    const struct wlr_output_info *out = get_output();
    WCHAR title[320], wname[64], wdesc[256];

    /* Build a Windows-style title: "Monitor and Compositor Properties" */
    if (out && out->description[0])
    {
        MultiByteToWideChar(CP_UTF8, 0, out->description, -1, wdesc, 256);
        swprintf(title, ARRAY_SIZE(title), L"%s Properties", wdesc);
    }
    else if (out)
    {
        MultiByteToWideChar(CP_UTF8, 0, out->name, -1, wname, 64);
        swprintf(title, ARRAY_SIZE(title), L"%s Properties", wname);
    }
    else
        swprintf(title, ARRAY_SIZE(title), L"Advanced Display Settings");

    {
        PROPSHEETPAGEW pages[] =
        {
            {
                .dwSize      = sizeof(PROPSHEETPAGEW),
                .dwFlags     = PSP_USETITLE,
                .hInstance   = module,
                .pszTemplate = MAKEINTRESOURCEW(IDD_ADAPTER),
                .pszTitle    = L"Adapter",
                .pfnDlgProc  = adapter_dialog_proc,
            },
            {
                .dwSize      = sizeof(PROPSHEETPAGEW),
                .dwFlags     = PSP_USETITLE,
                .hInstance   = module,
                .pszTemplate = MAKEINTRESOURCEW(IDD_MONITOR),
                .pszTitle    = L"Monitor",
                .pfnDlgProc  = monitor_dialog_proc,
            },
            {
                .dwSize      = sizeof(PROPSHEETPAGEW),
                .dwFlags     = PSP_USETITLE,
                .hInstance   = module,
                .pszTemplate = MAKEINTRESOURCEW(IDD_TROUBLESHOOT),
                .pszTitle    = L"Troubleshoot",
                .pfnDlgProc  = troubleshoot_dialog_proc,
            },
            {
                .dwSize      = sizeof(PROPSHEETPAGEW),
                .dwFlags     = PSP_USETITLE,
                .hInstance   = module,
                .pszTemplate = MAKEINTRESOURCEW(IDD_COLORMGMT),
                .pszTitle    = L"Color Management",
                .pfnDlgProc  = color_mgmt_dialog_proc,
            },
        };
        PROPSHEETHEADERW hdr =
        {
            .dwSize     = sizeof(PROPSHEETHEADERW),
            .dwFlags    = PSH_PROPSHEETPAGE,
            .hwndParent = parent,
            .hInstance  = module,
            .pszCaption = title,
            .nPages     = ARRAY_SIZE(pages),
            .nStartPage = 1,  /* open on Monitor tab */
            .ppsp       = pages,
        };

        PropertySheetW(&hdr);
    }

    /* After closing advanced settings, re-enumerate so the main page
     * reflects any changes made to refresh rate or VRR. */
    enumerate_outputs();
}

/* ================================================================== */
/* Identify — flash a number on each display                          */
/* ================================================================== */

static LRESULT CALLBACK identify_proc(HWND hwnd, UINT msg,
                                      WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_CREATE:
        SetTimer(hwnd, 1, 5000, NULL);  /* auto-dismiss after 5 seconds */
        return 0;

    case WM_TIMER:
        KillTimer(hwnd, 1);
        DestroyWindow(hwnd);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        RECT rc;
        HDC hdc = BeginPaint(hwnd, &ps);
        WCHAR label[16];
        HFONT big, old;

        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));

        swprintf(label, ARRAY_SIZE(label), L"%Iu",
                 (UINT_PTR)GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        big = CreateFontW(-(rc.bottom - rc.top) / 2, 0, 0, 0, FW_BOLD,
                          FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          DEFAULT_QUALITY, DEFAULT_PITCH, L"Ms Shell Dlg");
        old = SelectObject(hdc, big);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
        DeleteObject(big);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static void show_identify(void)
{
    unsigned int i, m;

    for (i = 0; i < wlr_data.num_outputs; i++)
    {
        const struct wlr_output_info *o = &wlr_data.outputs[i];
        unsigned int w = 0, h = 0;
        HWND win;

        if (!o->enabled) continue;

        for (m = 0; m < o->num_modes; m++)
            if (o->modes[m].current) { w = o->modes[m].width; h = o->modes[m].height; break; }
        if (!w && o->num_modes) { w = o->modes[0].width; h = o->modes[0].height; }
        if (!w) { w = 1920; h = 1080; }

        win = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                              L"DeskCplIdentify", NULL, WS_POPUP,
                              o->pos_x, o->pos_y, (int)w, (int)h,
                              NULL, NULL, module, NULL);
        if (!win) continue;
        SetWindowLongPtrW(win, GWLP_USERDATA, (LONG_PTR)(i + 1));  /* 1-based */
        ShowWindow(win, SW_SHOWNA);
        UpdateWindow(win);
    }
}

/* ================================================================== */
/* Main dialog procedure                                              */
/* ================================================================== */

static INT_PTR CALLBACK desktop_dialog_proc(HWND hwnd, UINT msg,
                                            WPARAM wparam, LPARAM lparam)
{
    TRACE("hwnd %p, msg %#x, wparam %#Ix, lparam %#Ix\n",
          hwnd, msg, wparam, lparam);

    switch (msg)
    {
    case WM_INITDIALOG:
        if (enumerate_outputs())
        {
            populate_output_combo(hwnd);
            populate_resolution_combo(hwnd);
            populate_orientation_combo(hwnd);
        }
        create_desktop_view(hwnd);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case IDC_OUTPUT_COMBO:
            if (HIWORD(wparam) == CBN_SELCHANGE)
            {
                int idx2 = (int)SendDlgItemMessageW(hwnd, IDC_OUTPUT_COMBO,
                                                    CB_GETCURSEL, 0, 0);
                if (idx2 >= 0)
                    sel_output = (int)SendDlgItemMessageW(hwnd, IDC_OUTPUT_COMBO,
                                                          CB_GETITEMDATA, idx2, 0);
                populate_resolution_combo(hwnd);
                populate_orientation_combo(hwnd);
                InvalidateRect(GetDlgItem(hwnd, IDC_VIRTUAL_DESKTOP), NULL, TRUE);
                SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
            }
            break;

        case IDC_RESOLUTION_COMBO:
            if (HIWORD(wparam) == CBN_SELCHANGE)
            {
                read_resolution_combo(hwnd);
                InvalidateRect(GetDlgItem(hwnd, IDC_VIRTUAL_DESKTOP), NULL, TRUE);
                SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
            }
            break;

        case IDC_ORIENTATION_COMBO:
            if (HIWORD(wparam) == CBN_SELCHANGE)
            {
                int idx2 = (int)SendDlgItemMessageW(hwnd, IDC_ORIENTATION_COMBO,
                                                    CB_GETCURSEL, 0, 0);
                if (idx2 >= 0)
                    staged_transform = (unsigned int)SendDlgItemMessageW(
                        hwnd, IDC_ORIENTATION_COMBO, CB_GETITEMDATA, idx2, 0);
                SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
            }
            break;

        case IDC_ADVANCED_BUTTON:
            open_advanced_settings(hwnd);
            /* Refresh combos — advanced may have changed refresh/VRR. */
            populate_resolution_combo(hwnd);
            populate_orientation_combo(hwnd);
            InvalidateRect(GetDlgItem(hwnd, IDC_VIRTUAL_DESKTOP), NULL, TRUE);
            break;

        case IDC_DETECT_BUTTON:
            /* Re-scan outputs and refresh the page. */
            if (enumerate_outputs())
            {
                populate_output_combo(hwnd);
                populate_resolution_combo(hwnd);
                populate_orientation_combo(hwnd);
                InvalidateRect(GetDlgItem(hwnd, IDC_VIRTUAL_DESKTOP), NULL, TRUE);
            }
            break;

        case IDC_IDENTIFY_BUTTON:
            show_identify();
            break;
        }
        return TRUE;

    case WM_NOTIFY:
    {
        NMHDR *nm = (NMHDR *)lparam;

        if (nm->code == PSN_APPLY)
        {
            if (apply_main_settings(hwnd))
                SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
            else
                SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, PSNRET_INVALID);
            return TRUE;
        }
        break;
    }
    }

    return FALSE;
}

/* ================================================================== */
/* Property-sheet boilerplate                                         */
/* ================================================================== */

static int CALLBACK property_sheet_callback(HWND hwnd, UINT msg,
                                            LPARAM lparam)
{
    TRACE("hwnd %p, msg %#x, lparam %#Ix\n", hwnd, msg, lparam);
    return 0;
}

static void create_property_sheets(HWND parent)
{
    INITCOMMONCONTROLSEX init =
    {
        .dwSize = sizeof(INITCOMMONCONTROLSEX),
        .dwICC  = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES,
    };
    PROPSHEETPAGEW pages[] =
    {
        {
            .dwSize      = sizeof(PROPSHEETPAGEW),
            .hInstance   = module,
            .pszTemplate = MAKEINTRESOURCEW(IDD_DESKTOP),
            .pfnDlgProc  = desktop_dialog_proc,
        },
    };
    PROPSHEETHEADERW header =
    {
        .dwSize     = sizeof(PROPSHEETHEADERW),
        .dwFlags    = PSH_PROPSHEETPAGE | PSH_USEICONID | PSH_USECALLBACK,
        .hwndParent = parent,
        .hInstance  = module,
        .pszCaption = MAKEINTRESOURCEW(IDS_CPL_NAME),
        .nPages     = ARRAY_SIZE(pages),
        .ppsp       = pages,
        .pfnCallback = property_sheet_callback,
    };
    ACTCTXW ctx =
    {
        .cbSize         = sizeof(ACTCTXW),
        .hModule        = module,
        .lpResourceName = MAKEINTRESOURCEW(124),
        .dwFlags        = ACTCTX_FLAG_HMODULE_VALID | ACTCTX_FLAG_RESOURCE_NAME_VALID,
    };
    ULONG_PTR cookie;
    HANDLE context;
    BOOL activated;

    OleInitialize(NULL);

    context = CreateActCtxW(&ctx);
    activated = (context != INVALID_HANDLE_VALUE) && ActivateActCtx(context, &cookie);

    InitCommonControlsEx(&init);
    PropertySheetW(&header);

    if (activated) DeactivateActCtx(0, cookie);
    ReleaseActCtx(context);
    OleUninitialize();
}

/* ================================================================== */
/* Window-class registration                                          */
/* ================================================================== */

static void register_window_class(void)
{
    WNDCLASSW cls =
    {
        .hInstance     = module,
        .lpfnWndProc  = desktop_view_proc,
        .lpszClassName = L"DeskCplDesktop",
    };
    WNDCLASSW idcls =
    {
        .hInstance     = module,
        .lpfnWndProc  = identify_proc,
        .lpszClassName = L"DeskCplIdentify",
        .hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH),
    };
    RegisterClassW(&cls);
    RegisterClassW(&idcls);
}

static void unregister_window_class(void)
{
    UnregisterClassW(L"DeskCplDesktop", module);
    UnregisterClassW(L"DeskCplIdentify", module);
}

/* ================================================================== */
/* CPlApplet — Control Panel entry point                              */
/* ================================================================== */

LONG CALLBACK CPlApplet(HWND hwnd, UINT command, LPARAM param1, LPARAM param2)
{
    TRACE("hwnd %p, command %u, param1 %#Ix, param2 %#Ix\n",
          hwnd, command, param1, param2);

    switch (command)
    {
    case CPL_INIT:
        register_window_class();
        return TRUE;

    case CPL_GETCOUNT:
        return 1;

    case CPL_INQUIRE:
    {
        CPLINFO *info = (CPLINFO *)param2;
        info->idIcon = ICO_MAIN;
        info->idName = IDS_CPL_NAME;
        info->idInfo = IDS_CPL_INFO;
        info->lData  = 0;
        return TRUE;
    }

    case CPL_DBLCLK:
        create_property_sheets(hwnd);
        break;

    case CPL_STOP:
        unregister_window_class();
        break;
    }

    return FALSE;
}

/* ================================================================== */
/* DllMain                                                            */
/* ================================================================== */

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    TRACE("instance %p, reason %ld, reserved %p\n", instance, reason, reserved);

    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        module = instance;
    }

    return TRUE;
}