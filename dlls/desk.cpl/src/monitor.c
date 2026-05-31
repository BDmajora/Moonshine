/* desk.cpl — Monitor tab (refresh rate, hide-modes, VRR, colors). */

#include "desk_private.h"
#include "monitor.h"
#include "display_state.h"
#include "shellapi.h"

WINE_DEFAULT_DEBUG_CHANNEL(deskcpl);

/* State for the monitor dialog — initialised on WM_INITDIALOG. */
static unsigned int mon_rates[MAX_WLR_MODES];
static unsigned int mon_rate_count;

INT_PTR CALLBACK monitor_dialog_proc(HWND hwnd, UINT msg,
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

            if (!out)
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

            if (!apply_wlr(&ap))
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