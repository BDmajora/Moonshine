/* desk.cpl — Advanced Settings property sheet (Adapter/Monitor/Troubleshoot/Color). */

#include "desk_private.h"
#include "advanced.h"
#include "adapter.h"
#include "monitor.h"
#include "display_state.h"
#include "shellapi.h"


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

void open_advanced_settings(HWND parent)
{
    const struct wlr_output_info *out = get_output();
    WCHAR title[320], wname[64], wdesc[256];

    /* Build a Windows-style title: "<monitor> Properties". */
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

    /* After closing, re-enumerate so the main page reflects any changes. */
    enumerate_outputs();
}