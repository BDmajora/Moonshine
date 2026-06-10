/* mmsys.cpl — Playback page, plus the shared endpoint-list engine that
 * recording.c reuses with eCapture.
 *
 * Each list row stores the endpoint id string (from IMMDevice::GetId) in
 * its lParam; rows are freed on refresh/destroy. "Set Default" goes through
 * mmdevapi's Wine-extension export SetDefaultAudioEndpoint (Phase 2). */

#include "mmsys_private.h"

#include "devpkey.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmsyscpl);

typedef HRESULT (WINAPI *set_default_endpoint_fn)(const WCHAR *id, EDataFlow flow, ERole role);

static set_default_endpoint_fn get_set_default_fn(void)
{
    HMODULE mmdevapi = GetModuleHandleW(L"mmdevapi.dll");
    if (!mmdevapi) mmdevapi = LoadLibraryW(L"mmdevapi.dll");
    if (!mmdevapi) return NULL;
    return (set_default_endpoint_fn)GetProcAddress(mmdevapi, "SetDefaultAudioEndpoint");
}

static void free_list_items(HWND list)
{
    int i, count = (int)SendMessageW(list, LVM_GETITEMCOUNT, 0, 0);

    for (i = 0; i < count; i++)
    {
        LVITEMW item = { .mask = LVIF_PARAM, .iItem = i };
        if (SendMessageW(list, LVM_GETITEMW, 0, (LPARAM)&item))
            CoTaskMemFree((void *)item.lParam);
    }
    SendMessageW(list, LVM_DELETEALLITEMS, 0, 0);
}

static WCHAR *selected_device_id(HWND hwnd)
{
    HWND list = GetDlgItem(hwnd, IDC_DEVICE_LIST);
    int sel = (int)SendMessageW(list, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    LVITEMW item = { .mask = LVIF_PARAM, .iItem = sel };

    if (sel < 0) return NULL;
    if (!SendMessageW(list, LVM_GETITEMW, 0, (LPARAM)&item)) return NULL;
    return (WCHAR *)item.lParam;
}

static IMMDevice *selected_device(HWND hwnd)
{
    IMMDeviceEnumerator *devenum;
    IMMDevice *dev = NULL;
    const WCHAR *id = selected_device_id(hwnd);

    if (!id) return NULL;
    if (!(devenum = create_enumerator())) return NULL;
    IMMDeviceEnumerator_GetDevice(devenum, id, &dev);
    IMMDeviceEnumerator_Release(devenum);
    return dev;
}

void endpoint_page_init(HWND hwnd, EDataFlow flow)
{
    HWND list = GetDlgItem(hwnd, IDC_DEVICE_LIST);
    LVCOLUMNW col;
    RECT rc;

    GetClientRect(list, &rc);

    SendMessageW(list, LVM_SETEXTENDEDLISTVIEWSTYLE,
                 LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (WCHAR *)L"Name";
    col.cx = (rc.right - rc.left) * 55 / 100;
    SendMessageW(list, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);

    col.pszText = (WCHAR *)L"Status";
    col.cx = (rc.right - rc.left) * 40 / 100;
    SendMessageW(list, LVM_INSERTCOLUMNW, 1, (LPARAM)&col);

    endpoint_page_refresh(hwnd, flow);
}

void endpoint_page_refresh(HWND hwnd, EDataFlow flow)
{
    HWND list = GetDlgItem(hwnd, IDC_DEVICE_LIST);
    IMMDeviceEnumerator *devenum;
    IMMDeviceCollection *devices = NULL;
    IMMDevice *def_dev = NULL;
    WCHAR *def_id = NULL;
    UINT i, count = 0;

    free_list_items(list);

    if (!(devenum = create_enumerator())) return;

    if (SUCCEEDED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(devenum, flow, eConsole, &def_dev)))
    {
        IMMDevice_GetId(def_dev, &def_id);
        IMMDevice_Release(def_dev);
    }

    if (FAILED(IMMDeviceEnumerator_EnumAudioEndpoints(devenum, flow,
                                                      DEVICE_STATE_ACTIVE, &devices)))
        goto done;

    IMMDeviceCollection_GetCount(devices, &count);

    for (i = 0; i < count; i++)
    {
        IMMDevice *dev = NULL;
        IPropertyStore *store = NULL;
        WCHAR *id = NULL;
        PROPVARIANT pv;
        LVITEMW item;
        BOOL is_default;

        if (FAILED(IMMDeviceCollection_Item(devices, i, &dev)))
            continue;

        if (FAILED(IMMDevice_GetId(dev, &id)) || !id)
        {
            IMMDevice_Release(dev);
            continue;
        }

        PropVariantInit(&pv);
        if (FAILED(IMMDevice_OpenPropertyStore(dev, STGM_READ, &store)) ||
            FAILED(IPropertyStore_GetValue(store, (const PROPERTYKEY *)&DEVPKEY_Device_FriendlyName, &pv)) ||
            pv.vt != VT_LPWSTR)
        {
            pv.vt = VT_LPWSTR;
            pv.pwszVal = NULL;
        }

        is_default = def_id && !lstrcmpW(def_id, id);

        memset(&item, 0, sizeof(item));
        item.mask    = LVIF_TEXT | LVIF_PARAM;
        item.iItem   = (int)i;
        item.pszText = pv.pwszVal ? pv.pwszVal :
                       (WCHAR *)(flow == eRender ? L"Speakers" : L"Microphone");
        item.lParam  = (LPARAM)id;   /* owned by the row; freed in free_list_items */
        item.iItem   = (int)SendMessageW(list, LVM_INSERTITEMW, 0, (LPARAM)&item);

        {
            LVITEMW sub = { .iSubItem = 1, .iItem = item.iItem, .mask = LVIF_TEXT };
            sub.pszText = (WCHAR *)(is_default ? L"Default Device" : L"Ready");
            SendMessageW(list, LVM_SETITEMTEXTW, item.iItem, (LPARAM)&sub);
        }

        if (is_default)
        {
            LVITEMW st = { .iItem = item.iItem,
                           .state = LVIS_SELECTED | LVIS_FOCUSED,
                           .stateMask = LVIS_SELECTED | LVIS_FOCUSED };
            SendMessageW(list, LVM_SETITEMSTATE, item.iItem, (LPARAM)&st);
        }

        if (store) IPropertyStore_Release(store);
        if (pv.pwszVal) PropVariantClear(&pv);
        IMMDevice_Release(dev);
    }

done:
    if (def_id) CoTaskMemFree(def_id);
    if (devices) IMMDeviceCollection_Release(devices);
    IMMDeviceEnumerator_Release(devenum);
}

void endpoint_page_destroy(HWND hwnd)
{
    free_list_items(GetDlgItem(hwnd, IDC_DEVICE_LIST));
}

void endpoint_page_command(HWND hwnd, EDataFlow flow, WPARAM wparam)
{
    switch (LOWORD(wparam))
    {
    case IDC_SET_DEFAULT:
    {
        const WCHAR *id = selected_device_id(hwnd);
        set_default_endpoint_fn set_default = get_set_default_fn();

        if (!id) break;
        if (!set_default)
        {
            MessageBoxW(hwnd, L"SetDefaultAudioEndpoint is not available in mmdevapi.",
                        L"Sound", MB_OK | MB_ICONWARNING);
            break;
        }
        if (FAILED(set_default(id, flow, eConsole)))
            MessageBoxW(hwnd, L"Failed to set the default device.",
                        L"Sound", MB_OK | MB_ICONWARNING);
        endpoint_page_refresh(hwnd, flow);
        break;
    }

    case IDC_PROPERTIES:
    {
        IMMDevice *dev = selected_device(hwnd);
        if (dev)
        {
            open_endpoint_properties(hwnd, dev);
            IMMDevice_Release(dev);
            endpoint_page_refresh(hwnd, flow);
        }
        break;
    }

    case IDC_CONFIGURE:
        /* Speaker-configuration wizard — not in v1. */
        MessageBoxW(hwnd, L"Speaker configuration is not available yet.",
                    L"Sound", MB_OK | MB_ICONINFORMATION);
        break;
    }
}

void endpoint_page_notify(HWND hwnd, EDataFlow flow, LPARAM lparam)
{
    NMHDR *nm = (NMHDR *)lparam;

    if (nm->idFrom == IDC_DEVICE_LIST && nm->code == NM_DBLCLK)
    {
        IMMDevice *dev = selected_device(hwnd);
        if (dev)
        {
            open_endpoint_properties(hwnd, dev);
            IMMDevice_Release(dev);
            endpoint_page_refresh(hwnd, flow);
        }
    }
    else if (nm->code == PSN_APPLY)
        SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
}

INT_PTR CALLBACK playback_dialog_proc(HWND hwnd, UINT msg,
                                      WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        endpoint_page_init(hwnd, eRender);
        return TRUE;

    case WM_COMMAND:
        endpoint_page_command(hwnd, eRender, wparam);
        return TRUE;

    case WM_NOTIFY:
        endpoint_page_notify(hwnd, eRender, lparam);
        return TRUE;

    case WM_DESTROY:
        endpoint_page_destroy(hwnd);
        return TRUE;
    }
    return FALSE;
}