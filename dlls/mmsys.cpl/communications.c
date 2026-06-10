/* mmsys.cpl — Communications tab.
 *
 * Single DWORD preference at
 *   HKCU\Software\Microsoft\Multimedia\Audio  value "UserDuckingPreference"
 *     0 = mute all other sounds
 *     1 = reduce by 80%   (Windows default)
 *     2 = reduce by 50%
 *     3 = do nothing
 * mmdevapi can consult this when communications-stream ducking lands. */

#include "mmsys_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmsyscpl);

static const WCHAR duck_key[]   = L"Software\\Microsoft\\Multimedia\\Audio";
static const WCHAR duck_value[] = L"UserDuckingPreference";

static DWORD read_ducking(void)
{
    HKEY key;
    DWORD val = 1, size = sizeof(val);   /* default: reduce by 80% */

    if (RegOpenKeyExW(HKEY_CURRENT_USER, duck_key, 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        RegQueryValueExW(key, duck_value, NULL, NULL, (BYTE *)&val, &size);
        RegCloseKey(key);
    }
    return (val <= 3) ? val : 1;
}

static void write_ducking(DWORD val)
{
    HKEY key;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, duck_key, 0, NULL, 0, KEY_WRITE,
                        NULL, &key, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(key, duck_value, 0, REG_DWORD, (const BYTE *)&val, sizeof(val));
        RegCloseKey(key);
    }
}

static const int radio_ids[] = { IDC_COMM_MUTE, IDC_COMM_80, IDC_COMM_50, IDC_COMM_NOTHING };

INT_PTR CALLBACK comms_dialog_proc(HWND hwnd, UINT msg,
                                   WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        CheckRadioButton(hwnd, IDC_COMM_MUTE, IDC_COMM_NOTHING,
                         radio_ids[read_ducking()]);
        return TRUE;

    case WM_COMMAND:
        if (HIWORD(wparam) == BN_CLICKED &&
            LOWORD(wparam) >= IDC_COMM_MUTE && LOWORD(wparam) <= IDC_COMM_NOTHING)
            SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
        return TRUE;

    case WM_NOTIFY:
    {
        NMHDR *nm = (NMHDR *)lparam;
        if (nm->code == PSN_APPLY)
        {
            unsigned int i;
            for (i = 0; i < ARRAY_SIZE(radio_ids); i++)
            {
                if (IsDlgButtonChecked(hwnd, radio_ids[i]) == BST_CHECKED)
                {
                    write_ducking(i);
                    break;
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
