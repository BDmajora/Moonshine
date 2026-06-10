/* mmsys.cpl — endpoint Properties, Levels tab.
 *
 * The Phase 2 proof: the slider drives
 * IAudioEndpointVolume::SetMasterVolumeLevelScalar (mmdevapi ->
 * winepipewire.drv -> PipeWire node volume) and an
 * IAudioEndpointVolumeCallback keeps the UI live when the volume changes
 * elsewhere. */

#include "mmsys_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmsyscpl);

#define WM_APP_VOLCHANGED (WM_APP + 1)

struct vol_callback
{
    IAudioEndpointVolumeCallback IAudioEndpointVolumeCallback_iface;
    LONG ref;
    HWND hwnd;
};

struct levels_page
{
    IMMDevice *dev;                 /* not owned */
    IAudioEndpointVolume *volume;
    struct vol_callback *callback;
    BOOL updating;                  /* guard: programmatic control updates */
};

/* ------------------------------------------------------------------ */
/* IAudioEndpointVolumeCallback                                       */
/* ------------------------------------------------------------------ */

static inline struct vol_callback *impl_from_cb(IAudioEndpointVolumeCallback *iface)
{
    return CONTAINING_RECORD(iface, struct vol_callback, IAudioEndpointVolumeCallback_iface);
}

static HRESULT WINAPI cb_QueryInterface(IAudioEndpointVolumeCallback *iface,
                                        REFIID riid, void **ppv)
{
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IAudioEndpointVolumeCallback))
    {
        *ppv = iface;
        IAudioEndpointVolumeCallback_AddRef(iface);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI cb_AddRef(IAudioEndpointVolumeCallback *iface)
{
    struct vol_callback *cb = impl_from_cb(iface);
    return InterlockedIncrement(&cb->ref);
}

static ULONG WINAPI cb_Release(IAudioEndpointVolumeCallback *iface)
{
    struct vol_callback *cb = impl_from_cb(iface);
    ULONG ref = InterlockedDecrement(&cb->ref);
    if (!ref) free(cb);
    return ref;
}

static HRESULT WINAPI cb_OnNotify(IAudioEndpointVolumeCallback *iface,
                                  AUDIO_VOLUME_NOTIFICATION_DATA *data)
{
    struct vol_callback *cb = impl_from_cb(iface);
    if (cb->hwnd && data)
        PostMessageW(cb->hwnd, WM_APP_VOLCHANGED,
                     (WPARAM)(int)(data->fMasterVolume * 100.0f + 0.5f),
                     (LPARAM)(data->bMuted ? 1 : 0));
    return S_OK;
}

static const IAudioEndpointVolumeCallbackVtbl cb_vtbl =
{
    cb_QueryInterface,
    cb_AddRef,
    cb_Release,
    cb_OnNotify,
};

/* ------------------------------------------------------------------ */
/* page                                                               */
/* ------------------------------------------------------------------ */

static void update_controls(HWND hwnd, struct levels_page *page, int pos, BOOL mute)
{
    WCHAR buf[16];

    page->updating = TRUE;
    SendDlgItemMessageW(hwnd, IDC_LVL_SLIDER, TBM_SETPOS, TRUE, pos);
    swprintf(buf, ARRAY_SIZE(buf), L"%d", pos);
    SetDlgItemTextW(hwnd, IDC_LVL_VALUE, buf);
    CheckDlgButton(hwnd, IDC_LVL_MUTE, mute ? BST_CHECKED : BST_UNCHECKED);
    page->updating = FALSE;
}

INT_PTR CALLBACK spk_levels_dialog_proc(HWND hwnd, UINT msg,
                                        WPARAM wparam, LPARAM lparam)
{
    struct levels_page *page = (struct levels_page *)GetWindowLongPtrW(hwnd, DWLP_USER);

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        PROPSHEETPAGEW *psp = (PROPSHEETPAGEW *)lparam;
        float scalar = 1.0f;
        BOOL mute = FALSE;
        HRESULT hr;

        page = calloc(1, sizeof(*page));
        if (!page) return TRUE;
        page->dev = (IMMDevice *)psp->lParam;
        SetWindowLongPtrW(hwnd, DWLP_USER, (LONG_PTR)page);

        SendDlgItemMessageW(hwnd, IDC_LVL_SLIDER, TBM_SETRANGE, TRUE, MAKELONG(0, 100));

        hr = IMMDevice_Activate(page->dev, &IID_IAudioEndpointVolume,
                                CLSCTX_INPROC_SERVER, NULL, (void **)&page->volume);
        if (FAILED(hr))
        {
            ERR("IAudioEndpointVolume activation failed: %08lx\n", hr);
            EnableWindow(GetDlgItem(hwnd, IDC_LVL_SLIDER), FALSE);
            EnableWindow(GetDlgItem(hwnd, IDC_LVL_MUTE), FALSE);
            return TRUE;
        }

        IAudioEndpointVolume_GetMasterVolumeLevelScalar(page->volume, &scalar);
        IAudioEndpointVolume_GetMute(page->volume, &mute);
        update_controls(hwnd, page, (int)(scalar * 100.0f + 0.5f), mute);

        page->callback = calloc(1, sizeof(*page->callback));
        if (page->callback)
        {
            page->callback->IAudioEndpointVolumeCallback_iface.lpVtbl = &cb_vtbl;
            page->callback->ref = 1;
            page->callback->hwnd = hwnd;
            IAudioEndpointVolume_RegisterControlChangeNotify(page->volume,
                    &page->callback->IAudioEndpointVolumeCallback_iface);
        }
        return TRUE;
    }

    case WM_HSCROLL:
        if (page && page->volume && !page->updating &&
            (HWND)lparam == GetDlgItem(hwnd, IDC_LVL_SLIDER))
        {
            int pos = (int)SendDlgItemMessageW(hwnd, IDC_LVL_SLIDER, TBM_GETPOS, 0, 0);
            WCHAR buf[16];

            IAudioEndpointVolume_SetMasterVolumeLevelScalar(page->volume,
                                                            pos / 100.0f, NULL);
            swprintf(buf, ARRAY_SIZE(buf), L"%d", pos);
            SetDlgItemTextW(hwnd, IDC_LVL_VALUE, buf);
        }
        return TRUE;

    case WM_COMMAND:
        if (page && page->volume && !page->updating)
        {
            switch (LOWORD(wparam))
            {
            case IDC_LVL_MUTE:
                IAudioEndpointVolume_SetMute(page->volume,
                        IsDlgButtonChecked(hwnd, IDC_LVL_MUTE) == BST_CHECKED, NULL);
                break;

            case IDC_LVL_BALANCE:
                MessageBoxW(hwnd, L"Per-channel balance is not available yet.",
                            L"Sound", MB_OK | MB_ICONINFORMATION);
                break;
            }
        }
        return TRUE;

    case WM_APP_VOLCHANGED:
        if (page)
            update_controls(hwnd, page, (int)wparam, lparam ? TRUE : FALSE);
        return TRUE;

    case WM_NOTIFY:
    {
        NMHDR *nm = (NMHDR *)lparam;
        if (nm->code == PSN_APPLY)
        {
            /* Volume/mute apply live; nothing staged. */
            SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        break;
    }

    case WM_DESTROY:
        if (page)
        {
            if (page->volume)
            {
                if (page->callback)
                {
                    page->callback->hwnd = NULL;
                    IAudioEndpointVolume_UnregisterControlChangeNotify(page->volume,
                            &page->callback->IAudioEndpointVolumeCallback_iface);
                    IAudioEndpointVolumeCallback_Release(
                            &page->callback->IAudioEndpointVolumeCallback_iface);
                }
                IAudioEndpointVolume_Release(page->volume);
            }
            free(page);
            SetWindowLongPtrW(hwnd, DWLP_USER, 0);
        }
        return TRUE;
    }
    return FALSE;
}
