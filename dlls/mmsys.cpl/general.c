/* mmsys.cpl — endpoint Properties, General tab.
 *
 * Shows friendly name / controller / jack info and the "Device usage"
 * enable-disable combo. Disabling writes DeviceState in the endpoint's
 * MMDevices registry key (the same value devenum reads back), which takes
 * effect for newly started applications. */

#include "mmsys_private.h"

#include "functiondiscoverykeys_devpkey.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmsyscpl);

struct general_page
{
    IMMDevice *dev;       /* not owned (sheet holds the reference) */
    DWORD initial_state;
};

/* Build the endpoint's registry Properties parent key path from its id:
 * id "{0.0.<flow>.00000000}.{guid}" ->
 *   Software\Microsoft\Windows\CurrentVersion\MMDevices\Audio\<Render|Capture>\{guid} */
static BOOL endpoint_reg_path(IMMDevice *dev, WCHAR *path, size_t count)
{
    WCHAR *id = NULL, *guid;
    EDataFlow flow = eRender;
    IMMEndpoint *ep;
    BOOL ret = FALSE;

    if (FAILED(IMMDevice_GetId(dev, &id)) || !id)
        return FALSE;

    if (SUCCEEDED(IMMDevice_QueryInterface(dev, &IID_IMMEndpoint, (void **)&ep)))
    {
        IMMEndpoint_GetDataFlow(ep, &flow);
        IMMEndpoint_Release(ep);
    }

    guid = wcsstr(id, L"}.{");
    if (guid)
    {
        swprintf(path, count,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\%s\\%s",
                 flow == eRender ? L"Render" : L"Capture", guid + 2);
        ret = TRUE;
    }
    CoTaskMemFree(id);
    return ret;
}

INT_PTR CALLBACK spk_general_dialog_proc(HWND hwnd, UINT msg,
                                         WPARAM wparam, LPARAM lparam)
{
    struct general_page *page = (struct general_page *)GetWindowLongPtrW(hwnd, DWLP_USER);

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        PROPSHEETPAGEW *psp = (PROPSHEETPAGEW *)lparam;
        IPropertyStore *store = NULL;
        PROPVARIANT pv;
        HWND usage;
        DWORD state = DEVICE_STATE_ACTIVE;

        page = calloc(1, sizeof(*page));
        if (!page) return TRUE;
        page->dev = (IMMDevice *)psp->lParam;
        SetWindowLongPtrW(hwnd, DWLP_USER, (LONG_PTR)page);

        PropVariantInit(&pv);
        if (SUCCEEDED(IMMDevice_OpenPropertyStore(page->dev, STGM_READ, &store)) &&
            SUCCEEDED(IPropertyStore_GetValue(store, (const PROPERTYKEY *)&DEVPKEY_Device_FriendlyName, &pv)) &&
            pv.vt == VT_LPWSTR && pv.pwszVal)
            SetDlgItemTextW(hwnd, IDC_GEN_NAME, pv.pwszVal);
        else
            SetDlgItemTextW(hwnd, IDC_GEN_NAME, L"Speakers");
        PropVariantClear(&pv);
        if (store) IPropertyStore_Release(store);

        SetDlgItemTextW(hwnd, IDC_GEN_CONTROLLER, L"YetiOS PipeWire Audio Device");
        SetDlgItemTextW(hwnd, IDC_GEN_JACK, L"Jack information is not available.");

        usage = GetDlgItem(hwnd, IDC_GEN_USAGE);
        SendMessageW(usage, CB_ADDSTRING, 0, (LPARAM)L"Use this device (enable)");
        SendMessageW(usage, CB_ADDSTRING, 0, (LPARAM)L"Don't use this device (disable)");

        IMMDevice_GetState(page->dev, &state);
        page->initial_state = state;
        SendMessageW(usage, CB_SETCURSEL, (state == DEVICE_STATE_ACTIVE) ? 0 : 1, 0);
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wparam) == IDC_GEN_USAGE && HIWORD(wparam) == CBN_SELCHANGE)
            SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
        return TRUE;

    case WM_NOTIFY:
    {
        NMHDR *nm = (NMHDR *)lparam;
        if (nm->code == PSN_APPLY && page)
        {
            int sel = (int)SendDlgItemMessageW(hwnd, IDC_GEN_USAGE, CB_GETCURSEL, 0, 0);
            DWORD want = (sel == 1) ? DEVICE_STATE_DISABLED : DEVICE_STATE_ACTIVE;

            if (want != page->initial_state)
            {
                WCHAR path[512];
                HKEY key;

                if (endpoint_reg_path(page->dev, path, ARRAY_SIZE(path)) &&
                    RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0,
                                  KEY_WRITE | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS)
                {
                    RegSetValueExW(key, L"DeviceState", 0, REG_DWORD,
                                   (const BYTE *)&want, sizeof(want));
                    RegCloseKey(key);
                    page->initial_state = want;
                    MessageBoxW(hwnd,
                                L"The device state will change for newly started applications.",
                                L"Sound", MB_OK | MB_ICONINFORMATION);
                }
            }
            SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        break;
    }

    case WM_DESTROY:
        free(page);
        SetWindowLongPtrW(hwnd, DWLP_USER, 0);
        return TRUE;
    }
    return FALSE;
}
