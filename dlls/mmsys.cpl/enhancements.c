/* mmsys.cpl — endpoint Properties, Enhancements tab.
 *
 * v1 persists the checkbox state only (HKCU\Software\Wine\Audio\Enhancements,
 * one DWORD bitmask per endpoint id). Wiring the bits to PipeWire
 * filter-chain modules (bass boost = low-shelf EQ, loudness = compressor,
 * virtual surround = HRIR convolver) is the planned stretch. */

#include "mmsys_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmsyscpl);

#define ENH_DISABLE_ALL  0x01
#define ENH_BASS         0x02
#define ENH_SURROUND     0x04
#define ENH_LOUDNESS     0x08
#define ENH_ROOM         0x10

static const WCHAR enh_key[] = L"Software\\Wine\\Audio\\Enhancements";

struct enh_page
{
    IMMDevice *dev;       /* not owned */
    WCHAR *id;            /* endpoint id = registry value name */
};

static DWORD read_mask(const WCHAR *id)
{
    HKEY key;
    DWORD val = 0, size = sizeof(val);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, enh_key, 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        RegQueryValueExW(key, id, NULL, NULL, (BYTE *)&val, &size);
        RegCloseKey(key);
    }
    return val;
}

static void write_mask(const WCHAR *id, DWORD val)
{
    HKEY key;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, enh_key, 0, NULL, 0, KEY_WRITE,
                        NULL, &key, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(key, id, 0, REG_DWORD, (const BYTE *)&val, sizeof(val));
        RegCloseKey(key);
    }
}

static void update_enable_state(HWND hwnd)
{
    BOOL all_off = IsDlgButtonChecked(hwnd, IDC_ENH_DISABLE_ALL) == BST_CHECKED;

    EnableWindow(GetDlgItem(hwnd, IDC_ENH_BASS), !all_off);
    EnableWindow(GetDlgItem(hwnd, IDC_ENH_SURROUND), !all_off);
    EnableWindow(GetDlgItem(hwnd, IDC_ENH_LOUDNESS), !all_off);
    EnableWindow(GetDlgItem(hwnd, IDC_ENH_ROOM), !all_off);
}

INT_PTR CALLBACK spk_enh_dialog_proc(HWND hwnd, UINT msg,
                                     WPARAM wparam, LPARAM lparam)
{
    struct enh_page *page = (struct enh_page *)GetWindowLongPtrW(hwnd, DWLP_USER);

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        PROPSHEETPAGEW *psp = (PROPSHEETPAGEW *)lparam;
        DWORD mask;

        page = calloc(1, sizeof(*page));
        if (!page) return TRUE;
        page->dev = (IMMDevice *)psp->lParam;
        IMMDevice_GetId(page->dev, &page->id);
        SetWindowLongPtrW(hwnd, DWLP_USER, (LONG_PTR)page);

        mask = page->id ? read_mask(page->id) : 0;
        CheckDlgButton(hwnd, IDC_ENH_DISABLE_ALL, (mask & ENH_DISABLE_ALL) ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_ENH_BASS,        (mask & ENH_BASS)        ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_ENH_SURROUND,    (mask & ENH_SURROUND)    ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_ENH_LOUDNESS,    (mask & ENH_LOUDNESS)    ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_ENH_ROOM,        (mask & ENH_ROOM)        ? BST_CHECKED : BST_UNCHECKED);
        update_enable_state(hwnd);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case IDC_ENH_DISABLE_ALL:
            update_enable_state(hwnd);
            /* fall through */
        case IDC_ENH_BASS:
        case IDC_ENH_SURROUND:
        case IDC_ENH_LOUDNESS:
        case IDC_ENH_ROOM:
            SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
            break;
        }
        return TRUE;

    case WM_NOTIFY:
    {
        NMHDR *nm = (NMHDR *)lparam;
        if (nm->code == PSN_APPLY && page && page->id)
        {
            DWORD mask = 0;
            if (IsDlgButtonChecked(hwnd, IDC_ENH_DISABLE_ALL) == BST_CHECKED) mask |= ENH_DISABLE_ALL;
            if (IsDlgButtonChecked(hwnd, IDC_ENH_BASS) == BST_CHECKED)        mask |= ENH_BASS;
            if (IsDlgButtonChecked(hwnd, IDC_ENH_SURROUND) == BST_CHECKED)    mask |= ENH_SURROUND;
            if (IsDlgButtonChecked(hwnd, IDC_ENH_LOUDNESS) == BST_CHECKED)    mask |= ENH_LOUDNESS;
            if (IsDlgButtonChecked(hwnd, IDC_ENH_ROOM) == BST_CHECKED)        mask |= ENH_ROOM;
            write_mask(page->id, mask);
            SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        break;
    }

    case WM_DESTROY:
        if (page)
        {
            CoTaskMemFree(page->id);
            free(page);
            SetWindowLongPtrW(hwnd, DWLP_USER, 0);
        }
        return TRUE;
    }
    return FALSE;
}
