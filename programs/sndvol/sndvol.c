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

#include <math.h>

#include <initguid.h>
#include "sndvol_private.h"

#include "devpkey.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

    /* flyout custom-draw state (full mixer leaves these zeroed) */
    float fly_level;
    BOOL  fly_mute;
    BOOL  dragging;
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

#define FLY_W            96
#define FLY_PAD          10
#define FLY_DEV          40      /* device-icon button (top) */
#define FLY_SLIDER_TOP   (FLY_PAD + FLY_DEV + 10)
#define FLY_SLIDER_H     176
#define FLY_MUTE_H       38      /* mute speaker button (bottom) */
#define FLY_LINK_H       22      /* "Mixer" link */
#define FLY_H  (FLY_SLIDER_TOP + FLY_SLIDER_H + 12 + FLY_MUTE_H + FLY_LINK_H + FLY_PAD)
#define FLY_RADIUS       8

struct fly_layout
{
    RECT dev, mute, link;
    int  cx, track_top, track_bottom;
};

static void fly_compute(struct fly_layout *L)
{
    int mt;

    L->cx = FLY_W / 2;
    SetRect(&L->dev, (FLY_W - FLY_DEV) / 2, FLY_PAD,
            (FLY_W + FLY_DEV) / 2, FLY_PAD + FLY_DEV);
    L->track_top    = FLY_SLIDER_TOP;
    L->track_bottom = FLY_SLIDER_TOP + FLY_SLIDER_H;
    mt = L->track_bottom + 12;
    SetRect(&L->mute, (FLY_W - FLY_MUTE_H) / 2, mt,
            (FLY_W + FLY_MUTE_H) / 2, mt + FLY_MUTE_H);
    SetRect(&L->link, 0, L->mute.bottom + 4, FLY_W, L->mute.bottom + 4 + FLY_LINK_H);
}

static int fly_thumb_y(const struct fly_layout *L, float level)
{
    if (level < 0.f) level = 0.f;
    if (level > 1.f) level = 1.f;
    return L->track_bottom - (int)(level * (L->track_bottom - L->track_top) + 0.5f);
}

/* One right-opening sound-wave arc (a ")" curve), centred at (cxc,cy).
 * The pen is expected to be already selected into dc. */
static void draw_wave_arc(HDC dc, int cxc, int cy, int r)
{
    POINT pts[13];
    int i, n = ARRAY_SIZE(pts);
    const double ha = 52.0 * M_PI / 180.0;   /* half sweep */

    for (i = 0; i < n; i++)
    {
        double a = -ha + (2.0 * ha) * i / (n - 1);   /* y is down */
        pts[i].x = cxc + (int)(r * cos(a) + 0.5);
        pts[i].y = cy  + (int)(r * sin(a) + 0.5);
    }
    Polyline(dc, pts, n);
}

/* The Windows 7 volume speaker: a blue cone with curved sound waves whose
 * count tracks the level (none at 0, up to three near full); a red
 * prohibition badge instead of waves when muted. Used for the mute button. */
static void draw_volume_speaker(HDC dc, const RECT *rc, float level, BOOL muted)
{
    int w = rc->right - rc->left, h = rc->bottom - rc->top;
    int cy = rc->top + h / 2;
    int boxL = rc->left + w * 3 / 16;
    int boxR = rc->left + w * 6 / 16;
    int boxT = cy - h * 2 / 16, boxB = cy + h * 2 / 16;
    int coneR = rc->left + w * 9 / 16;
    int coneT = cy - h * 5 / 16, coneB = cy + h * 5 / 16;
    COLORREF blue = RGB(28, 78, 140);
    HBRUSH br = CreateSolidBrush(blue);
    HPEN pen = CreatePen(PS_SOLID, max(1, w / 22), blue);
    HGDIOBJ ob, op;
    POINT cone[4];

    op = SelectObject(dc, pen);
    ob = SelectObject(dc, br);

    /* cone (magnet box + flared trapezoid) */
    Rectangle(dc, boxL, boxT, boxR + 1, boxB + 1);
    cone[0].x = boxR;  cone[0].y = boxT;
    cone[1].x = coneR; cone[1].y = coneT;
    cone[2].x = coneR; cone[2].y = coneB;
    cone[3].x = boxR;  cone[3].y = boxB;
    Polygon(dc, cone, 4);

    SelectObject(dc, GetStockObject(NULL_BRUSH));   /* arcs/badge stroked only */
    DeleteObject(br);

    if (muted)
    {
        HPEN rp = CreatePen(PS_SOLID, max(2, w / 14), RGB(206, 42, 42));
        int rr  = h * 4 / 16;
        int rcx = coneR + w * 4 / 16;
        int off = (int)(rr * 0.70);

        SelectObject(dc, rp);
        Ellipse(dc, rcx - rr, cy - rr, rcx + rr, cy + rr);
        MoveToEx(dc, rcx - off, cy + off, NULL);
        LineTo(dc, rcx + off, cy - off);
        SelectObject(dc, pen);
        DeleteObject(rp);
    }
    else
    {
        int waves = 0, i;
        if (level > 0.001f) waves = 1;
        if (level > 0.34f)  waves = 2;
        if (level > 0.67f)  waves = 3;
        for (i = 1; i <= waves; i++)
            draw_wave_arc(dc, coneR, cy, h * (1 + 2 * i) / 16);
    }

    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(pen);
}

/* The playback-device icon shown at the top of the flyout: a small silver
 * speaker cabinet with a woofer + tweeter. Deliberately distinct from the
 * blue volume glyph below, matching Windows 7's device thumbnail. */
static void draw_device_icon(HDC dc, const RECT *rc)
{
    int w = rc->right - rc->left, h = rc->bottom - rc->top;
    RECT cab = { rc->left + w * 3 / 16, rc->top + h / 16,
                 rc->right - w * 3 / 16, rc->bottom - h / 16 };
    int cabw = cab.right - cab.left, cabh = cab.bottom - cab.top;
    int cx = (cab.left + cab.right) / 2;
    int wy = cab.top + cabh * 62 / 100;
    int r  = cabw * 30 / 100;
    int ty = cab.top + cabh * 22 / 100;
    int tr = cabw * 10 / 100;
    HPEN border, op;
    HBRUSH b;
    HGDIOBJ ob;
    int y;

    /* cabinet: light-to-darker silver vertical gradient */
    for (y = cab.top; y < cab.bottom; y++)
    {
        int t = (y - cab.top) * 255 / (cabh ? cabh : 1);
        int c = 0xE8 - (0xE8 - 0xB6) * t / 255;
        RECT ln = { cab.left, y, cab.right, y + 1 };
        b = CreateSolidBrush(RGB(c, c, c));
        FillRect(dc, &ln, b);
        DeleteObject(b);
    }

    border = CreatePen(PS_SOLID, 1, RGB(0x6E, 0x72, 0x78));
    op = SelectObject(dc, border);
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, cab.left, cab.top, cab.right, cab.bottom, 4, 4);

    /* woofer: outer ring + filled cone */
    Ellipse(dc, cx - r, wy - r, cx + r, wy + r);
    b = CreateSolidBrush(RGB(0x9C, 0xA0, 0xA6));
    ob = SelectObject(dc, b);
    Ellipse(dc, cx - r / 2, wy - r / 2, cx + r / 2, wy + r / 2);
    SelectObject(dc, ob);
    DeleteObject(b);

    /* tweeter */
    Ellipse(dc, cx - tr, ty - tr, cx + tr, ty + tr);

    SelectObject(dc, op);
    DeleteObject(border);
}

static void fly_paint(HWND hwnd, struct mixer_wnd *w)
{
    PAINTSTRUCT ps;
    HDC hdc, dc;
    HBITMAP bmp, oldbmp;
    RECT rc, fill;
    struct fly_layout L;
    HBRUSH b;
    HPEN border, oldpen;
    HFONT oldfont;
    int y, ty;

    hdc = BeginPaint(hwnd, &ps);
    GetClientRect(hwnd, &rc);
    fly_compute(&L);

    dc = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    oldbmp = SelectObject(dc, bmp);

    /* panel: subtle vertical gradient, light Aero tint */
    for (y = 0; y < rc.bottom; y++)
    {
        int t = MulDiv(y, 255, rc.bottom ? rc.bottom : 1);
        int r  = 0xF6 - (0xF6 - 0xE6) * t / 255;
        int g  = 0xF8 - (0xF8 - 0xEC) * t / 255;
        int bl = 0xFB - (0xFB - 0xF2) * t / 255;
        RECT ln = { 0, y, rc.right, y + 1 };
        b = CreateSolidBrush(RGB(r, g, bl));
        FillRect(dc, &ln, b);
        DeleteObject(b);
    }

    /* outer border, matching the rounded window region */
    border = CreatePen(PS_SOLID, 1, RGB(0x9A, 0xA4, 0xB2));
    oldpen = SelectObject(dc, border);
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, 0, 0, rc.right, rc.bottom, FLY_RADIUS * 2, FLY_RADIUS * 2);
    SelectObject(dc, oldpen);
    DeleteObject(border);

    /* device icon */
    draw_device_icon(dc, &L.dev);

    /* slider channel */
    SetRect(&fill, L.cx - 2, L.track_top, L.cx + 3, L.track_bottom);
    b = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(dc, &fill, b);
    DeleteObject(b);
    border = CreatePen(PS_SOLID, 1, RGB(0xB4, 0xBC, 0xC6));
    oldpen = SelectObject(dc, border);
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, L.cx - 2, L.track_top, L.cx + 3, L.track_bottom);
    SelectObject(dc, oldpen);
    DeleteObject(border);

    /* blue fill below the thumb */
    ty = fly_thumb_y(&L, w->fly_level);
    for (y = ty; y < L.track_bottom - 1; y++)
    {
        int t  = (L.track_bottom - y) * 255 / (L.track_bottom - L.track_top);
        int r  = 0x2E + (0x66 - 0x2E) * t / 255;
        int g  = 0x8C + (0xBC - 0x8C) * t / 255;
        int bl = 0xE0 + (0xF4 - 0xE0) * t / 255;
        RECT ln = { L.cx - 1, y, L.cx + 2, y + 1 };
        b = CreateSolidBrush(RGB(r, g, bl));
        FillRect(dc, &ln, b);
        DeleteObject(b);
    }

    /* thumb: light handle with a dark border */
    {
        RECT th = { L.cx - 11, ty - 6, L.cx + 12, ty + 7 };
        int yy;
        for (yy = th.top; yy < th.bottom; yy++)
        {
            int t = (yy - th.top) * 255 / (th.bottom - th.top);
            int c = 0xFF - (0xFF - 0xDD) * t / 255;
            RECT ln = { th.left, yy, th.right, yy + 1 };
            b = CreateSolidBrush(RGB(c, c, c));
            FillRect(dc, &ln, b);
            DeleteObject(b);
        }
        border = CreatePen(PS_SOLID, 1, RGB(0x5E, 0x68, 0x76));
        oldpen = SelectObject(dc, border);
        SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, th.left, th.top, th.right, th.bottom, 4, 4);
        SelectObject(dc, oldpen);
        DeleteObject(border);
    }

    /* mute button glyph */
    draw_volume_speaker(dc, &L.mute, w->fly_level, w->fly_mute);

    /* "Mixer" link */
    {
        HFONT f = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                              0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(0x21, 0x5C, 0xAF));
        oldfont = SelectObject(dc, f ? f : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
        DrawTextW(dc, L"Mixer", -1, &L.link, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, oldfont);
        if (f) DeleteObject(f);
    }

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldbmp);
    DeleteObject(bmp);
    DeleteDC(dc);
    EndPaint(hwnd, &ps);
}

static void fly_apply_level(struct mixer_wnd *w, float level)
{
    if (level < 0.f) level = 0.f;
    if (level > 1.f) level = 1.f;
    w->fly_level = level;
    if (w->master)
        IAudioEndpointVolume_SetMasterVolumeLevelScalar(w->master, level, NULL);
}

static void fly_level_from_y(HWND hwnd, struct mixer_wnd *w, int py)
{
    struct fly_layout L;
    float level;

    fly_compute(&L);
    level = (float)(L.track_bottom - py) / (float)(L.track_bottom - L.track_top);
    fly_apply_level(w, level);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void launch_cmd(const WCHAR *cmdline)
{
    WCHAR buf[MAX_PATH];
    STARTUPINFOW si = { .cb = sizeof(si) };
    PROCESS_INFORMATION pi;

    lstrcpynW(buf, cmdline, ARRAY_SIZE(buf));
    if (CreateProcessW(NULL, buf, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

static void launch_self_mixer(void)
{
    WCHAR path[MAX_PATH];
    STARTUPINFOW si = { .cb = sizeof(si) };
    PROCESS_INFORMATION pi;

    GetModuleFileNameW(NULL, path, ARRAY_SIZE(path));
    if (CreateProcessW(path, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

static LRESULT CALLBACK flyout_wndproc(HWND hwnd, UINT msg,
                                       WPARAM wparam, LPARAM lparam)
{
    struct mixer_wnd *w = (struct mixer_wnd *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg)
    {
    case WM_CREATE:
    {
        float scalar;
        BOOL mute;

        w = calloc(1, sizeof(*w));
        if (!w) return -1;
        w->flyout = TRUE;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)w);

        if (FAILED(bind_endpoint(w, hwnd)))
            return -1;

        if (SUCCEEDED(IAudioEndpointVolume_GetMasterVolumeLevelScalar(w->master, &scalar)))
            w->fly_level = scalar;
        if (SUCCEEDED(IAudioEndpointVolume_GetMute(w->master, &mute)))
            w->fly_mute = mute;
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;   /* fully painted in WM_PAINT (double-buffered) */

    case WM_PAINT:
        if (w) fly_paint(hwnd, w);
        return 0;

    case WM_LBUTTONDOWN:
    {
        POINT pt = { (short)LOWORD(lparam), (short)HIWORD(lparam) };
        struct fly_layout L;

        if (!w) return 0;
        fly_compute(&L);

        if (pt.x >= L.cx - 16 && pt.x <= L.cx + 16 &&
            pt.y >= L.track_top - 8 && pt.y <= L.track_bottom + 8)
        {
            w->dragging = TRUE;
            SetCapture(hwnd);
            fly_level_from_y(hwnd, w, pt.y);
            return 0;
        }
        if (PtInRect(&L.mute, pt))
        {
            w->fly_mute = !w->fly_mute;
            if (w->master) IAudioEndpointVolume_SetMute(w->master, w->fly_mute, NULL);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (PtInRect(&L.dev, pt))
        {
            launch_cmd(L"control.exe mmsys.cpl,,0");
            return 0;
        }
        if (PtInRect(&L.link, pt))
        {
            launch_self_mixer();
            DestroyWindow(hwnd);
            return 0;
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        if (w && w->dragging) fly_level_from_y(hwnd, w, (short)HIWORD(lparam));
        return 0;

    case WM_LBUTTONUP:
        if (w && w->dragging) { w->dragging = FALSE; ReleaseCapture(); }
        return 0;

    case WM_MOUSEWHEEL:
        if (w)
        {
            int d = GET_WHEEL_DELTA_WPARAM(wparam);
            fly_apply_level(w, w->fly_level + (d > 0 ? 0.02f : -0.02f));
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_APP_VOLCHANGED:
        if (w)
        {
            w->fly_level = (int)wparam / 100.0f;
            w->fly_mute  = lparam ? TRUE : FALSE;
            if (!w->dragging) InvalidateRect(hwnd, NULL, FALSE);
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
    HRGN rgn;
    MSG msg;
    int x, y;

    wc.lpfnWndProc   = flyout_wndproc;
    wc.hInstance     = instance;
    wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"YetiVolumeFlyout";
    RegisterClassW(&wc);

    /* Bottom-right of the work area — above the tray clock */
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    x = work.right - FLY_W - 6;
    y = work.bottom - FLY_H - 6;

    hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"YetiVolumeFlyout", L"Volume",
        WS_POPUP,
        x, y, FLY_W, FLY_H,
        NULL, NULL, instance, NULL);
    if (!hwnd) return 1;

    rgn = CreateRoundRectRgn(0, 0, FLY_W + 1, FLY_H + 1, FLY_RADIUS * 2, FLY_RADIUS * 2);
    SetWindowRgn(hwnd, rgn, TRUE);   /* window owns rgn now */

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