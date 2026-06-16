/* sndvol.exe — Volume Mixer.
 *
 * Two faces, Windows 7 style:
 *
 *   sndvol.exe          — full mixer: Device column (endpoint master via
 *                         IAudioEndpointVolume) + one column per audio
 *                         session (ISimpleAudioVolume). Sessions are
 *                         re-snapshotted on a 1s timer so rows appear and
 *                         disappear live as apps start/stop playing.
 *
 *   sndvol.exe /flyout  — compact topmost popup (one master slider, mute,
 *                         "Mixer" button) for the tray applet to summon.
 *                         Dismisses itself on focus loss, like Windows.
 *
 * Master slider -> IAudioEndpointVolume -> winepipewire.drv node volume.
 * Session slider -> ISimpleAudioVolume -> per-stream software gain.
 * The two are independent by construction.
 */

#include <initguid.h>
#include "sndvol_private.h"

#include "devpkey.h"

WINE_DEFAULT_DEBUG_CHANNEL(sndvol);

HINSTANCE instance;

#define ICO_SNDVOL 100

#define WM_APP_VOLCHANGED (WM_APP + 1)

#define MAX_COLUMNS 16

/* control ids */
#define IDC_MASTER_SLIDER  500
#define IDC_MASTER_MUTE    501
#define IDC_OPEN_MIXER     502
#define IDC_SESSION_BASE   600   /* + i*2: slider, + i*2 + 1: mute */

/* full-mixer layout */
#define COL_W       100
#define COL_GAP     8
#define DEV_GAP     20      /* extra gap between device and app columns */
#define MARGIN      12
#define LABEL_H     32
#define SLIDER_H    160
#define MUTE_H      20

/* ------------------------------------------------------------------ */
/* IAudioEndpointVolumeCallback — posts WM_APP_VOLCHANGED              */
/* ------------------------------------------------------------------ */

struct vol_callback
{
    IAudioEndpointVolumeCallback IAudioEndpointVolumeCallback_iface;
    LONG ref;
    HWND hwnd;
};

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
/* Window state                                                       */
/* ------------------------------------------------------------------ */

struct mixer_wnd
{
    BOOL flyout;
    BOOL updating;                     /* programmatic slider updates */

    IMMDevice *dev;
    WCHAR dev_name[128];
    IAudioEndpointVolume *master;
    struct vol_callback *callback;

    struct session_info *sessions;
    int count;

    /* full-mixer per-column controls (sessions only) */
    HWND s_label[MAX_COLUMNS];
    HWND s_slider[MAX_COLUMNS];
    HWND s_mute[MAX_COLUMNS];

    HWND master_slider;
    HWND master_mute;
};

/* ------------------------------------------------------------------ */
/* Endpoint binding                                                   */
/* ------------------------------------------------------------------ */

static HRESULT bind_endpoint(struct mixer_wnd *w, HWND hwnd)
{
    IMMDeviceEnumerator *devenum = NULL;
    IPropertyStore *store = NULL;
    PROPVARIANT pv;
    HRESULT hr;

    lstrcpyW(w->dev_name, L"Speakers");

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IMMDeviceEnumerator, (void **)&devenum);
    if (FAILED(hr)) return hr;

    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(devenum, eRender,
                                                     eConsole, &w->dev);
    IMMDeviceEnumerator_Release(devenum);
    if (FAILED(hr)) return hr;

    PropVariantInit(&pv);
    if (SUCCEEDED(IMMDevice_OpenPropertyStore(w->dev, STGM_READ, &store)))
    {
        if (SUCCEEDED(IPropertyStore_GetValue(store,
                (const PROPERTYKEY *)&DEVPKEY_Device_FriendlyName, &pv)) &&
            pv.vt == VT_LPWSTR && pv.pwszVal)
            lstrcpynW(w->dev_name, pv.pwszVal, ARRAY_SIZE(w->dev_name));
        PropVariantClear(&pv);
        IPropertyStore_Release(store);
    }

    hr = IMMDevice_Activate(w->dev, &IID_IAudioEndpointVolume,
                            CLSCTX_INPROC_SERVER, NULL, (void **)&w->master);
    if (FAILED(hr))
    {
        ERR("IAudioEndpointVolume activation failed: %08lx\n", hr);
        return hr;
    }

    w->callback = calloc(1, sizeof(*w->callback));
    if (w->callback)
    {
        w->callback->IAudioEndpointVolumeCallback_iface.lpVtbl = &cb_vtbl;
        w->callback->ref = 1;
        w->callback->hwnd = hwnd;
        IAudioEndpointVolume_RegisterControlChangeNotify(w->master,
                &w->callback->IAudioEndpointVolumeCallback_iface);
    }
    return S_OK;
}

static void unbind_endpoint(struct mixer_wnd *w)
{
    free_sessions(w->sessions, w->count);
    w->sessions = NULL;
    w->count = 0;

    if (w->master)
    {
        if (w->callback)
        {
            w->callback->hwnd = NULL;
            IAudioEndpointVolume_UnregisterControlChangeNotify(w->master,
                    &w->callback->IAudioEndpointVolumeCallback_iface);
            IAudioEndpointVolumeCallback_Release(
                    &w->callback->IAudioEndpointVolumeCallback_iface);
            w->callback = NULL;
        }
        IAudioEndpointVolume_Release(w->master);
        w->master = NULL;
    }
    if (w->dev) { IMMDevice_Release(w->dev); w->dev = NULL; }
}

/* ------------------------------------------------------------------ */
/* Slider helpers (trackbars are top=0, so invert)                    */
/* ------------------------------------------------------------------ */

static HWND make_slider(HWND parent, int id, int x, int y)
{
    HWND tb = CreateWindowW(TRACKBAR_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | TBS_VERT | TBS_BOTH | TBS_NOTICKS,
        x, y, 36, SLIDER_H, parent, (HMENU)(INT_PTR)id, instance, NULL);
    SendMessageW(tb, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
    SendMessageW(tb, TBM_SETPAGESIZE, 0, 10);
    return tb;
}

static void slider_set(HWND tb, float scalar)
{
    SendMessageW(tb, TBM_SETPOS, TRUE, 100 - (int)(scalar * 100.0f + 0.5f));
}

static int slider_get(HWND tb)
{
    return 100 - (int)SendMessageW(tb, TBM_GETPOS, 0, 0);
}

/* ------------------------------------------------------------------ */
/* Full mixer — session columns                                       */
/* ------------------------------------------------------------------ */

static int session_col_x(int i)
{
    return MARGIN + COL_W + DEV_GAP + i * (COL_W + COL_GAP);
}

static void destroy_session_controls(struct mixer_wnd *w)
{
    int i;
    for (i = 0; i < MAX_COLUMNS; i++)
    {
        if (w->s_label[i])  { DestroyWindow(w->s_label[i]);  w->s_label[i] = NULL; }
        if (w->s_slider[i]) { DestroyWindow(w->s_slider[i]); w->s_slider[i] = NULL; }
        if (w->s_mute[i])   { DestroyWindow(w->s_mute[i]);   w->s_mute[i] = NULL; }
    }
}

static void rebuild_session_controls(HWND hwnd, struct mixer_wnd *w)
{
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    RECT rc;
    int i, n = min(w->count, MAX_COLUMNS);
    int client_w = session_col_x(n) - COL_GAP + MARGIN;
    int client_h = MARGIN + LABEL_H + SLIDER_H + 6 + MUTE_H + MARGIN;

    destroy_session_controls(w);

    for (i = 0; i < n; i++)
    {
        int x = session_col_x(i);
        int y = MARGIN;

        w->s_label[i] = CreateWindowW(L"STATIC", w->sessions[i].label,
            WS_CHILD | WS_VISIBLE | SS_CENTER | SS_ENDELLIPSIS,
            x, y, COL_W, LABEL_H, hwnd, NULL, instance, NULL);
        SendMessageW(w->s_label[i], WM_SETFONT, (WPARAM)font, TRUE);
        y += LABEL_H;

        w->s_slider[i] = make_slider(hwnd, IDC_SESSION_BASE + i * 2,
                                     x + (COL_W - 36) / 2, y);
        y += SLIDER_H + 6;

        w->s_mute[i] = CreateWindowW(L"BUTTON", L"Mute",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x + (COL_W - 60) / 2, y, 60, MUTE_H,
            hwnd, (HMENU)(INT_PTR)(IDC_SESSION_BASE + i * 2 + 1),
            instance, NULL);
        SendMessageW(w->s_mute[i], WM_SETFONT, (WPARAM)font, TRUE);

        if (!w->sessions[i].volume)
        {
            EnableWindow(w->s_slider[i], FALSE);
            EnableWindow(w->s_mute[i], FALSE);
        }
    }

    /* Resize the window to fit the columns */
    SetRect(&rc, 0, 0, client_w, client_h);
    AdjustWindowRect(&rc, (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE), FALSE);
    SetWindowPos(hwnd, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/* Pull current values into every slider/mute. Skips the control being
 * dragged so the poll never fights the user's thumb. */
static void update_values(struct mixer_wnd *w)
{
    HWND captured = GetCapture();
    float scalar;
    BOOL mute;
    int i;

    w->updating = TRUE;

    if (w->master)
    {
        if (w->master_slider && w->master_slider != captured &&
            SUCCEEDED(IAudioEndpointVolume_GetMasterVolumeLevelScalar(w->master, &scalar)))
            slider_set(w->master_slider, scalar);
        if (w->master_mute &&
            SUCCEEDED(IAudioEndpointVolume_GetMute(w->master, &mute)))
            SendMessageW(w->master_mute, BM_SETCHECK,
                         mute ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    for (i = 0; i < min(w->count, MAX_COLUMNS); i++)
    {
        ISimpleAudioVolume *vol = w->sessions[i].volume;
        if (!vol) continue;
        if (w->s_slider[i] && w->s_slider[i] != captured &&
            SUCCEEDED(ISimpleAudioVolume_GetMasterVolume(vol, &scalar)))
            slider_set(w->s_slider[i], scalar);
        if (w->s_mute[i] &&
            SUCCEEDED(ISimpleAudioVolume_GetMute(vol, &mute)))
            SendMessageW(w->s_mute[i], BM_SETCHECK,
                         mute ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    w->updating = FALSE;
}

static void poll_sessions(HWND hwnd, struct mixer_wnd *w)
{
    struct session_info *fresh = NULL;
    int n = snapshot_sessions(w->dev, &fresh);

    if (n < 0) return;

    if (!sessions_equal(fresh, n, w->sessions, w->count))
    {
        free_sessions(w->sessions, w->count);
        w->sessions = fresh;
        w->count = n;
        rebuild_session_controls(hwnd, w);
    }
    else
        free_sessions(fresh, n);

    update_values(w);
}

/* ------------------------------------------------------------------ */
/* Input handling shared by both windows                              */
/* ------------------------------------------------------------------ */

static void handle_vscroll(struct mixer_wnd *w, HWND ctl)
{
    int pos, i;

    if (w->updating || !ctl) return;
    pos = slider_get(ctl);

    if (ctl == w->master_slider)
    {
        if (w->master)
            IAudioEndpointVolume_SetMasterVolumeLevelScalar(w->master,
                                                            pos / 100.0f, NULL);
        return;
    }
    for (i = 0; i < min(w->count, MAX_COLUMNS); i++)
    {
        if (ctl == w->s_slider[i] && w->sessions[i].volume)
        {
            ISimpleAudioVolume_SetMasterVolume(w->sessions[i].volume,
                                               pos / 100.0f, NULL);
            return;
        }
    }
}

static void handle_mute(struct mixer_wnd *w, int id, HWND ctl)
{
    BOOL checked = SendMessageW(ctl, BM_GETCHECK, 0, 0) == BST_CHECKED;

    if (w->updating) return;

    if (id == IDC_MASTER_MUTE)
    {
        if (w->master)
            IAudioEndpointVolume_SetMute(w->master, checked, NULL);
        return;
    }
    if (id >= IDC_SESSION_BASE && (id - IDC_SESSION_BASE) & 1)
    {
        int i = (id - IDC_SESSION_BASE) / 2;
        if (i < min(w->count, MAX_COLUMNS) && w->sessions[i].volume)
            ISimpleAudioVolume_SetMute(w->sessions[i].volume, checked, NULL);
    }
}

/* ------------------------------------------------------------------ */
/* Full mixer window                                                  */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK mixer_wndproc(HWND hwnd, UINT msg,
                                      WPARAM wparam, LPARAM lparam)
{
    struct mixer_wnd *w = (struct mixer_wnd *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_CREATE:
    {
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HWND label;

        w = calloc(1, sizeof(*w));
        if (!w) return -1;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)w);

        if (FAILED(bind_endpoint(w, hwnd)))
        {
            MessageBoxW(hwnd, L"No audio device is available.",
                        L"Volume Mixer", MB_OK | MB_ICONWARNING);
            return -1;
        }

        /* Device column */
        label = CreateWindowW(L"STATIC", L"Device",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            MARGIN, MARGIN, COL_W, LABEL_H, hwnd, NULL, instance, NULL);
        SendMessageW(label, WM_SETFONT, (WPARAM)font, TRUE);

        w->master_slider = make_slider(hwnd, IDC_MASTER_SLIDER,
            MARGIN + (COL_W - 36) / 2, MARGIN + LABEL_H);

        w->master_mute = CreateWindowW(L"BUTTON", L"Mute",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            MARGIN + (COL_W - 60) / 2, MARGIN + LABEL_H + SLIDER_H + 6,
            60, MUTE_H, hwnd, (HMENU)IDC_MASTER_MUTE, instance, NULL);
        SendMessageW(w->master_mute, WM_SETFONT, (WPARAM)font, TRUE);

        poll_sessions(hwnd, w);
        SetTimer(hwnd, 1, 1000, NULL);
        return 0;
    }

    case WM_TIMER:
        if (w) poll_sessions(hwnd, w);
        return 0;

    case WM_VSCROLL:
        if (w) handle_vscroll(w, (HWND)lparam);
        return 0;

    case WM_COMMAND:
        if (w && HIWORD(wparam) == BN_CLICKED)
            handle_mute(w, LOWORD(wparam), (HWND)lparam);
        return 0;

    case WM_APP_VOLCHANGED:
        if (w && w->master_slider)
        {
            w->updating = TRUE;
            slider_set(w->master_slider, (int)wparam / 100.0f);
            SendMessageW(w->master_mute, BM_SETCHECK,
                         lparam ? BST_CHECKED : BST_UNCHECKED, 0);
            w->updating = FALSE;
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (w)
        {
            KillTimer(hwnd, 1);
            unbind_endpoint(w);
            free(w);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int show_mixer(void)
{
    WNDCLASSW wc = {0};
    WCHAR title[192];
    HWND hwnd;
    MSG msg;
    RECT rc;
    struct mixer_wnd *w;

    wc.lpfnWndProc   = mixer_wndproc;
    wc.hInstance     = instance;
    wc.hIcon         = LoadIconW(instance, MAKEINTRESOURCEW(ICO_SNDVOL));
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"YetiVolumeMixer";
    RegisterClassW(&wc);

    /* Initial size: device column only; WM_CREATE/poll resizes for rows. */
    SetRect(&rc, 0, 0,
            MARGIN + COL_W + DEV_GAP + MARGIN,
            MARGIN + LABEL_H + SLIDER_H + 6 + MUTE_H + MARGIN);
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);

    hwnd = CreateWindowW(L"YetiVolumeMixer", L"Volume Mixer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, instance, NULL);
    if (!hwnd) return 1;

    /* Title with the endpoint friendly name, Windows style */
    w = (struct mixer_wnd *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (w)
    {
        swprintf(title, ARRAY_SIZE(title), L"Volume Mixer - %s", w->dev_name);
        SetWindowTextW(hwnd, title);
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Flyout window                                                      */
/* ------------------------------------------------------------------ */

#define FLY_W 88
#define FLY_H (MARGIN + SLIDER_H + 6 + MUTE_H + 6 + 24 + MARGIN)

static LRESULT CALLBACK flyout_wndproc(HWND hwnd, UINT msg,
                                       WPARAM wparam, LPARAM lparam)
{
    struct mixer_wnd *w = (struct mixer_wnd *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_CREATE:
    {
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HWND btn;

        w = calloc(1, sizeof(*w));
        if (!w) return -1;
        w->flyout = TRUE;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)w);

        if (FAILED(bind_endpoint(w, hwnd)))
            return -1;

        w->master_slider = make_slider(hwnd, IDC_MASTER_SLIDER,
                                       (FLY_W - 36) / 2, MARGIN);

        w->master_mute = CreateWindowW(L"BUTTON", L"Mute",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            (FLY_W - 60) / 2, MARGIN + SLIDER_H + 6, 60, MUTE_H,
            hwnd, (HMENU)IDC_MASTER_MUTE, instance, NULL);
        SendMessageW(w->master_mute, WM_SETFONT, (WPARAM)font, TRUE);

        btn = CreateWindowW(L"BUTTON", L"Mixer",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            (FLY_W - 64) / 2, MARGIN + SLIDER_H + 6 + MUTE_H + 6, 64, 24,
            hwnd, (HMENU)IDC_OPEN_MIXER, instance, NULL);
        SendMessageW(btn, WM_SETFONT, (WPARAM)font, TRUE);

        update_values(w);
        return 0;
    }

    case WM_VSCROLL:
        if (w) handle_vscroll(w, (HWND)lparam);
        return 0;

    case WM_COMMAND:
        if (!w) return 0;
        if (LOWORD(wparam) == IDC_OPEN_MIXER)
        {
            WCHAR path[MAX_PATH];
            STARTUPINFOW si = { .cb = sizeof(si) };
            PROCESS_INFORMATION pi;

            GetModuleFileNameW(NULL, path, ARRAY_SIZE(path));
            if (CreateProcessW(path, NULL, NULL, NULL, FALSE, 0, NULL, NULL,
                               &si, &pi))
            {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
            DestroyWindow(hwnd);
            return 0;
        }
        if (HIWORD(wparam) == BN_CLICKED)
            handle_mute(w, LOWORD(wparam), (HWND)lparam);
        return 0;

    case WM_APP_VOLCHANGED:
        if (w && w->master_slider)
        {
            w->updating = TRUE;
            slider_set(w->master_slider, (int)wparam / 100.0f);
            SendMessageW(w->master_mute, BM_SETCHECK,
                         lparam ? BST_CHECKED : BST_UNCHECKED, 0);
            w->updating = FALSE;
        }
        return 0;

    case WM_ACTIVATE:
        /* Windows flyout behavior: dismiss on focus loss */
        if (LOWORD(wparam) == WA_INACTIVE)
            DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (w)
        {
            unbind_endpoint(w);
            free(w);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int show_flyout(void)
{
    WNDCLASSW wc = {0};
    RECT work = {0, 0, 800, 600};
    HWND hwnd;
    MSG msg;
    int x, y;

    wc.lpfnWndProc   = flyout_wndproc;
    wc.hInstance     = instance;
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"YetiVolumeFlyout";
    RegisterClassW(&wc);

    /* Bottom-right of the work area — above the tray clock */
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    x = work.right - FLY_W - 8;
    y = work.bottom - FLY_H - 8;

    hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"YetiVolumeFlyout", L"Volume",
        WS_POPUP | WS_BORDER,
        x, y, FLY_W, FLY_H,
        NULL, NULL, instance, NULL);
    if (!hwnd) return 1;

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmdline, int show)
{
    INITCOMMONCONTROLSEX init =
    {
        .dwSize = sizeof(init),
        .dwICC  = ICC_BAR_CLASSES,
    };
    BOOL flyout = FALSE;
    int ret;

    instance = inst;

    if (cmdline && (wcsstr(cmdline, L"/flyout") || wcsstr(cmdline, L"-flyout")))
        flyout = TRUE;

    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
        return 1;
    InitCommonControlsEx(&init);

    ret = flyout ? show_flyout() : show_mixer();

    CoUninitialize();
    return ret;
}