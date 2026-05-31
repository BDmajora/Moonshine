/* desk.cpl — main Display page: apply staged settings + dialog procedure. */

#include "desk_private.h"
#include "main_page.h"
#include "combos.h"
#include "preview.h"
#include "identify.h"
#include "advanced.h"
#include "display_state.h"

WINE_DEFAULT_DEBUG_CHANNEL(deskcpl);

static BOOL apply_main_settings(HWND hwnd)
{
    const struct wlr_output_info *out = get_output();
    struct wlr_apply_params ap;

    if (!out) return FALSE;

    memset(&ap, 0, sizeof(ap));
    lstrcpynA(ap.output_name, out->name, sizeof(ap.output_name));
    ap.flags     = WLR_APPLY_MODE | WLR_APPLY_TRANSFORM;
    ap.width     = staged_w;
    ap.height    = staged_h;
    ap.transform = staged_transform;

    TRACE("Main apply: %ux%u transform=%u on %s\n",
          staged_w, staged_h, staged_transform, out->name);

    if (!apply_wlr(&ap))
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

INT_PTR CALLBACK desktop_dialog_proc(HWND hwnd, UINT msg,
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

        case IDC_MAIN_DISPLAY_CHECK:
            /* stub: set selected output as primary — implemented later. */
            SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
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