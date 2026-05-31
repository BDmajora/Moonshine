/* desk.cpl — Adapter tab and its List All Modes dialog. */

#include "desk_private.h"
#include "adapter.h"
#include "display_state.h"
#include "shellapi.h"


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

            if (out && sel >= 0)
            {
                unsigned int mi = (unsigned int)SendMessageW(lb, LB_GETITEMDATA, sel, 0);
                struct wlr_apply_params ap;

                memset(&ap, 0, sizeof(ap));
                lstrcpynA(ap.output_name, out->name, sizeof(ap.output_name));
                ap.flags       = WLR_APPLY_MODE | WLR_APPLY_REFRESH;
                ap.width       = out->modes[mi].width;
                ap.height      = out->modes[mi].height;
                ap.refresh_mhz = out->modes[mi].refresh_mhz;
                if (apply_wlr(&ap))
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

INT_PTR CALLBACK adapter_dialog_proc(HWND hwnd, UINT msg,
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