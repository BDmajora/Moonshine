/* mmsys.cpl — Sounds tab.
 *
 * Per-event .wav mapping in the standard Windows registry layout:
 *   HKCU\AppEvents\EventLabels\<event>                 (default) = label
 *   HKCU\AppEvents\Schemes\Apps\.Default\<event>\.Current (default) = path
 *
 * v1: fixed event list, Browse/Test, staged changes written on Apply.
 * Scheme save-as is not implemented yet; the combo shows the current
 * scheme only. */

#include "mmsys_private.h"

#include "mmsystem.h"
#include "commdlg.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmsyscpl);

static const struct
{
    const WCHAR *key;     /* HKCU\AppEvents subkey */
    const WCHAR *label;   /* fallback if EventLabels has none */
}
event_table[] =
{
    { L".Default",           L"Default Beep" },
    { L"SystemAsterisk",     L"Asterisk" },
    { L"SystemExclamation",  L"Exclamation" },
    { L"SystemHand",         L"Critical Stop" },
    { L"SystemNotification", L"Notification" },
    { L"SystemQuestion",     L"Question" },
    { L"WindowsLogon",       L"Windows Logon" },
    { L"WindowsLogoff",      L"Windows Logoff" },
    { L"DeviceConnect",      L"Device Connect" },
    { L"DeviceDisconnect",   L"Device Disconnect" },
    { L"MailBeep",           L"New Mail Notification" },
};

/* Staged (unapplied) path edits, parallel to event_table. */
static WCHAR staged_paths[ARRAY_SIZE(event_table)][MAX_PATH];
static BOOL  staged_dirty[ARRAY_SIZE(event_table)];

static void read_event_path(unsigned int idx, WCHAR *path, DWORD size_bytes)
{
    WCHAR key[256];
    HKEY hkey;

    path[0] = 0;
    swprintf(key, ARRAY_SIZE(key),
             L"AppEvents\\Schemes\\Apps\\.Default\\%s\\.Current", event_table[idx].key);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &hkey) == ERROR_SUCCESS)
    {
        RegQueryValueExW(hkey, NULL, NULL, NULL, (BYTE *)path, &size_bytes);
        RegCloseKey(hkey);
    }
}

static void write_event_path(unsigned int idx, const WCHAR *path)
{
    WCHAR key[256];
    HKEY hkey;

    swprintf(key, ARRAY_SIZE(key),
             L"AppEvents\\Schemes\\Apps\\.Default\\%s\\.Current", event_table[idx].key);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_WRITE, NULL,
                        &hkey, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(hkey, NULL, 0, REG_SZ, (const BYTE *)path,
                       (lstrlenW(path) + 1) * sizeof(WCHAR));
        RegCloseKey(hkey);
    }
}

static void read_event_label(unsigned int idx, WCHAR *label, DWORD size_bytes)
{
    WCHAR key[256];
    HKEY hkey;

    lstrcpynW(label, event_table[idx].label, size_bytes / sizeof(WCHAR));
    swprintf(key, ARRAY_SIZE(key), L"AppEvents\\EventLabels\\%s", event_table[idx].key);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &hkey) == ERROR_SUCCESS)
    {
        RegQueryValueExW(hkey, NULL, NULL, NULL, (BYTE *)label, &size_bytes);
        RegCloseKey(hkey);
    }
}

static int selected_event(HWND hwnd)
{
    return (int)SendDlgItemMessageW(hwnd, IDC_EVENT_LIST, LB_GETCURSEL, 0, 0);
}

static void show_selected_path(HWND hwnd)
{
    int sel = selected_event(hwnd);
    BOOL has = sel >= 0;

    SetDlgItemTextW(hwnd, IDC_SOUND_PATH, has ? staged_paths[sel] : L"");
    EnableWindow(GetDlgItem(hwnd, IDC_BROWSE), has);
    EnableWindow(GetDlgItem(hwnd, IDC_TEST), has && staged_paths[sel][0]);
}

INT_PTR CALLBACK sounds_dialog_proc(HWND hwnd, UINT msg,
                                    WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        unsigned int i;

        SendDlgItemMessageW(hwnd, IDC_SCHEME_COMBO, CB_ADDSTRING, 0,
                            (LPARAM)L"Windows Default");
        SendDlgItemMessageW(hwnd, IDC_SCHEME_COMBO, CB_SETCURSEL, 0, 0);

        for (i = 0; i < ARRAY_SIZE(event_table); i++)
        {
            WCHAR label[128];
            read_event_label(i, label, sizeof(label));
            SendDlgItemMessageW(hwnd, IDC_EVENT_LIST, LB_ADDSTRING, 0, (LPARAM)label);
            read_event_path(i, staged_paths[i], sizeof(staged_paths[i]));
            staged_dirty[i] = FALSE;
        }
        SendDlgItemMessageW(hwnd, IDC_EVENT_LIST, LB_SETCURSEL, 0, 0);
        show_selected_path(hwnd);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case IDC_EVENT_LIST:
            if (HIWORD(wparam) == LBN_SELCHANGE)
                show_selected_path(hwnd);
            break;

        case IDC_BROWSE:
        {
            int sel = selected_event(hwnd);
            WCHAR path[MAX_PATH] = L"";
            OPENFILENAMEW ofn =
            {
                .lStructSize = sizeof(ofn),
                .hwndOwner   = hwnd,
                .lpstrFilter = L"Wave files (*.wav)\0*.wav\0All files (*.*)\0*.*\0",
                .lpstrFile   = path,
                .nMaxFile    = ARRAY_SIZE(path),
                .Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST,
            };

            if (sel < 0) break;
            if (GetOpenFileNameW(&ofn))
            {
                lstrcpynW(staged_paths[sel], path, ARRAY_SIZE(staged_paths[sel]));
                staged_dirty[sel] = TRUE;
                show_selected_path(hwnd);
                SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
            }
            break;
        }

        case IDC_TEST:
        {
            int sel = selected_event(hwnd);
            if (sel >= 0 && staged_paths[sel][0])
                PlaySoundW(staged_paths[sel], NULL, SND_FILENAME | SND_ASYNC);
            break;
        }
        }
        return TRUE;

    case WM_NOTIFY:
    {
        NMHDR *nm = (NMHDR *)lparam;
        if (nm->code == PSN_APPLY)
        {
            unsigned int i;
            for (i = 0; i < ARRAY_SIZE(event_table); i++)
            {
                if (!staged_dirty[i]) continue;
                write_event_path(i, staged_paths[i]);
                staged_dirty[i] = FALSE;
            }
            SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}