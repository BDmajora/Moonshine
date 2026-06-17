/* audiotray.exe — notification-area volume applet.
 *
 * A small background PE process started at shell startup. It owns a hidden
 * top-level window, registers a Shell_NotifyIcon that the frostedglass systray
 * host renders beside the clock, and binds to the default render endpoint's
 * IAudioEndpointVolume.
 *
 *   Left-click   -> sndvol.exe /flyout   (the compact slider)
 *   Right-click  -> context menu (full mixer, Playback/Recording/Sounds tabs)
 *   Tooltip      -> endpoint friendly name + current level / muted
 *
 * The icon updates live: an IAudioEndpointVolumeCallback fires on any volume
 * or mute change (made anywhere), and an IMMNotificationClient re-binds when
 * the default endpoint itself changes. Both callbacks only PostMessage to the
 * UI thread, which re-reads state and redraws — no COM work on foreign threads.
 *
 * The speaker glyph is drawn with GDI at the system small-icon size and rebuilt
 * on every state change, so there are no bitmap assets to ship and the
 * mute/level overlay is always exact.
 */

#define COBJMACROS
#define INITGUID

#include "windows.h"
#include "shellapi.h"
#include "propsys.h"
#include "mmdeviceapi.h"
#include "endpointvolume.h"
#include "functiondiscoverykeys_devpkey.h"

#include "audiotray.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define TRAY_ICON_ID     1

#define WM_APP_TRAYICON   (WM_APP + 1)   /* Shell_NotifyIcon callback        */
#define WM_APP_VOLCHANGED (WM_APP + 2)   /* posted from OnNotify             */
#define WM_APP_REBIND     (WM_APP + 3)   /* posted from OnDefaultDeviceChanged*/

/* ------------------------------------------------------------------ */
/* Globals                                                            */
/* ------------------------------------------------------------------ */

typedef struct
{
    HINSTANCE hInstance;
    HWND      hMainWnd;
    HMENU     hMenu;
    HICON     hIcon;                       /* owned; destroyed on replace */
    NOTIFYICONDATAW nid;
    UINT      msgTaskbarCreated;

    IMMDeviceEnumerator  *enumerator;
    IMMDevice            *device;
    IAudioEndpointVolume *endpoint;

    IAudioEndpointVolumeCallback vol_cb_iface;
    IMMNotificationClient        notif_iface;
    LONG vol_cb_ref;
    LONG notif_ref;

    float level;
    BOOL  muted;
    WCHAR device_name[128];
} TRAY_GLOBALS;

static TRAY_GLOBALS Globals;

/* ------------------------------------------------------------------ */
/* Icon rendering                                                     */
/* ------------------------------------------------------------------ */

static void draw_speaker(HDC dc, int w, int h, float level, BOOL muted)
{
    const COLORREF body = RGB(58, 64, 74);
    HBRUSH br  = CreateSolidBrush(body);
    HPEN   pen = CreatePen(PS_SOLID, 1, body);
    HGDIOBJ ob = SelectObject(dc, br);
    HGDIOBJ op = SelectObject(dc, pen);
    int cy    = h / 2;
    int magL  = MulDiv(w, 1, 16);
    int magR  = MulDiv(w, 4, 16);
    int magT  = MulDiv(h, 6, 16);
    int magB  = MulDiv(h, 10, 16);
    int coneR = MulDiv(w, 8, 16);
    int coneT = MulDiv(h, 3, 16);
    int coneB = MulDiv(h, 13, 16);
    POINT cone[4];

    /* magnet + cone (one speaker shape) */
    Rectangle(dc, magL, magT, magR + 1, magB + 1);
    cone[0].x = magR;  cone[0].y = magT;
    cone[1].x = coneR; cone[1].y = coneT;
    cone[2].x = coneR; cone[2].y = coneB;
    cone[3].x = magR;  cone[3].y = magB;
    Polygon(dc, cone, 4);

    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
    DeleteObject(br);

    if (muted)
    {
        HPEN rp = CreatePen(PS_SOLID, max(1, MulDiv(w, 1, 16) + 1), RGB(200, 40, 40));
        HGDIOBJ orp = SelectObject(dc, rp);
        int x0 = coneR + MulDiv(w, 1, 16);
        int x1 = w - MulDiv(w, 1, 16);
        int yy = MulDiv(h, 3, 16);

        MoveToEx(dc, x0, yy, NULL);     LineTo(dc, x1, h - yy);
        MoveToEx(dc, x0, h - yy, NULL); LineTo(dc, x1, yy);

        SelectObject(dc, orp);
        DeleteObject(rp);
    }
    else
    {
        int waves = 0, i;
        HPEN wp, owp;

        if (level > 0.001f) waves = 1;
        if (level > 0.34f)  waves = 2;
        if (level > 0.67f)  waves = 3;

        wp  = CreatePen(PS_SOLID, max(1, MulDiv(w, 1, 16)), body);
        owp = SelectObject(dc, wp);
        for (i = 0; i < waves; i++)
        {
            int x = coneR + MulDiv(w, 1, 16) + i * MulDiv(w, 2, 16);
            int d = MulDiv(h, 2 + i, 16);
            int e = MulDiv(w, 1, 16) + 1;
            MoveToEx(dc, x, cy - d, NULL);
            LineTo(dc, x + e, cy);
            LineTo(dc, x, cy + d);
        }
        SelectObject(dc, owp);
        DeleteObject(wp);
    }
}

static HICON build_tray_icon(float level, BOOL muted)
{
    const COLORREF KEY = RGB(255, 0, 255);   /* transparent colour key */
    int w = GetSystemMetrics(SM_CXSMICON);
    int h = GetSystemMetrics(SM_CYSMICON);
    HDC screen, dcColor, dcMask;
    HBITMAP color, mask, oldC, oldM;
    HBRUSH bg;
    RECT rc;
    ICONINFO ii;
    HICON icon;

    if (w <= 0) w = 16;
    if (h <= 0) h = 16;

    screen  = GetDC(NULL);
    dcColor = CreateCompatibleDC(screen);
    dcMask  = CreateCompatibleDC(screen);
    color   = CreateCompatibleBitmap(screen, w, h);
    mask    = CreateBitmap(w, h, 1, 1, NULL);
    ReleaseDC(NULL, screen);

    oldC = SelectObject(dcColor, color);
    oldM = SelectObject(dcMask, mask);

    bg = CreateSolidBrush(KEY);
    rc.left = rc.top = 0; rc.right = w; rc.bottom = h;
    FillRect(dcColor, &rc, bg);
    DeleteObject(bg);

    draw_speaker(dcColor, w, h, level, muted);

    /* AND mask: key pixels -> 1 (transparent), everything else -> 0 */
    SetBkColor(dcColor, KEY);
    BitBlt(dcMask, 0, 0, w, h, dcColor, 0, 0, SRCCOPY);

    /* Force the transparent regions of the colour bitmap to black so the
     * XOR pass leaves the background untouched there. */
    SetTextColor(dcColor, RGB(255, 255, 255));
    SetBkColor(dcColor, RGB(0, 0, 0));
    BitBlt(dcColor, 0, 0, w, h, dcMask, 0, 0, SRCAND);

    SelectObject(dcColor, oldC);
    SelectObject(dcMask, oldM);
    DeleteDC(dcColor);
    DeleteDC(dcMask);

    ii.fIcon    = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmMask  = mask;
    ii.hbmColor = color;
    icon = CreateIconIndirect(&ii);

    DeleteObject(color);
    DeleteObject(mask);
    return icon;
}

/* ------------------------------------------------------------------ */
/* Tooltip + tray update                                              */
/* ------------------------------------------------------------------ */

static void build_tooltip(TRAY_GLOBALS *g, int pct, BOOL mute)
{
    WCHAR fmt[64], buf[160], dev[110];

    lstrcpynW(dev, g->device_name[0] ? g->device_name : L"Speakers", ARRAY_SIZE(dev));
    LoadStringW(g->hInstance, mute ? IDS_TIP_MUTED : IDS_TIP_LEVEL, fmt, ARRAY_SIZE(fmt));

    if (mute) wsprintfW(buf, fmt, dev);
    else      wsprintfW(buf, fmt, dev, pct);

    lstrcpynW(g->nid.szTip, buf, ARRAY_SIZE(g->nid.szTip));
}

static void refresh_tray(TRAY_GLOBALS *g)
{
    float level = 0.f;
    BOOL  mute  = FALSE;
    HICON old   = g->hIcon;
    int   pct;

    if (g->endpoint)
    {
        if (FAILED(IAudioEndpointVolume_GetMasterVolumeLevelScalar(g->endpoint, &level)))
            level = 0.f;
        IAudioEndpointVolume_GetMute(g->endpoint, &mute);
    }

    g->level = level;
    g->muted = mute;
    pct = (int)(level * 100.f + 0.5f);

    g->hIcon     = build_tray_icon(level, mute);
    g->nid.hIcon = g->hIcon;
    build_tooltip(g, pct, mute);

    g->nid.uFlags = NIF_ICON | NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g->nid);

    if (old) DestroyIcon(old);
}

static void add_tray(TRAY_GLOBALS *g)
{
    ZeroMemory(&g->nid, sizeof(g->nid));
    g->nid.cbSize           = sizeof(g->nid);
    g->nid.hWnd             = g->hMainWnd;
    g->nid.uID              = TRAY_ICON_ID;
    g->nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g->nid.uCallbackMessage = WM_APP_TRAYICON;

    if (!g->hIcon) g->hIcon = build_tray_icon(g->level, g->muted);
    g->nid.hIcon = g->hIcon;
    build_tooltip(g, (int)(g->level * 100.f + 0.5f), g->muted);

    Shell_NotifyIconW(NIM_ADD, &g->nid);
}

/* ------------------------------------------------------------------ */
/* IAudioEndpointVolumeCallback                                       */
/* ------------------------------------------------------------------ */

static inline TRAY_GLOBALS *globals_from_volcb(IAudioEndpointVolumeCallback *iface)
{
    return CONTAINING_RECORD(iface, TRAY_GLOBALS, vol_cb_iface);
}

static HRESULT WINAPI volcb_QueryInterface(IAudioEndpointVolumeCallback *iface,
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

static ULONG WINAPI volcb_AddRef(IAudioEndpointVolumeCallback *iface)
{
    return InterlockedIncrement(&globals_from_volcb(iface)->vol_cb_ref);
}

static ULONG WINAPI volcb_Release(IAudioEndpointVolumeCallback *iface)
{
    /* Embedded in the process-lifetime Globals; never freed here. */
    return InterlockedDecrement(&globals_from_volcb(iface)->vol_cb_ref);
}

static HRESULT WINAPI volcb_OnNotify(IAudioEndpointVolumeCallback *iface,
                                     AUDIO_VOLUME_NOTIFICATION_DATA *data)
{
    TRAY_GLOBALS *g = globals_from_volcb(iface);
    if (g->hMainWnd)
        PostMessageW(g->hMainWnd, WM_APP_VOLCHANGED, 0, 0);
    return S_OK;
}

static const IAudioEndpointVolumeCallbackVtbl vol_cb_vtbl =
{
    volcb_QueryInterface,
    volcb_AddRef,
    volcb_Release,
    volcb_OnNotify,
};

/* ------------------------------------------------------------------ */
/* IMMNotificationClient (re-bind on default-device change)           */
/* ------------------------------------------------------------------ */

static inline TRAY_GLOBALS *globals_from_notif(IMMNotificationClient *iface)
{
    return CONTAINING_RECORD(iface, TRAY_GLOBALS, notif_iface);
}

static HRESULT WINAPI notif_QueryInterface(IMMNotificationClient *iface,
                                           REFIID riid, void **ppv)
{
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IMMNotificationClient))
    {
        *ppv = iface;
        IMMNotificationClient_AddRef(iface);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI notif_AddRef(IMMNotificationClient *iface)
{
    return InterlockedIncrement(&globals_from_notif(iface)->notif_ref);
}

static ULONG WINAPI notif_Release(IMMNotificationClient *iface)
{
    return InterlockedDecrement(&globals_from_notif(iface)->notif_ref);
}

static HRESULT WINAPI notif_OnDeviceStateChanged(IMMNotificationClient *iface,
                                                 LPCWSTR id, DWORD state)
{
    return S_OK;
}

static HRESULT WINAPI notif_OnDeviceAdded(IMMNotificationClient *iface, LPCWSTR id)
{
    return S_OK;
}

static HRESULT WINAPI notif_OnDeviceRemoved(IMMNotificationClient *iface, LPCWSTR id)
{
    return S_OK;
}

static HRESULT WINAPI notif_OnDefaultDeviceChanged(IMMNotificationClient *iface,
                                                   EDataFlow flow, ERole role,
                                                   LPCWSTR id)
{
    TRAY_GLOBALS *g = globals_from_notif(iface);
    if (flow == eRender && role == eConsole && g->hMainWnd)
        PostMessageW(g->hMainWnd, WM_APP_REBIND, 0, 0);
    return S_OK;
}

static HRESULT WINAPI notif_OnPropertyValueChanged(IMMNotificationClient *iface,
                                                   LPCWSTR id, const PROPERTYKEY key)
{
    return S_OK;
}

static const IMMNotificationClientVtbl notif_vtbl =
{
    notif_QueryInterface,
    notif_AddRef,
    notif_Release,
    notif_OnDeviceStateChanged,
    notif_OnDeviceAdded,
    notif_OnDeviceRemoved,
    notif_OnDefaultDeviceChanged,
    notif_OnPropertyValueChanged,
};

/* ------------------------------------------------------------------ */
/* Endpoint binding                                                   */
/* ------------------------------------------------------------------ */

static HRESULT bind_endpoint(TRAY_GLOBALS *g)
{
    IPropertyStore *props = NULL;
    PROPVARIANT pv;
    HRESULT hr;

    if (!g->enumerator) return E_UNEXPECTED;

    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(g->enumerator, eRender,
                                                     eConsole, &g->device);
    if (FAILED(hr)) { g->device = NULL; return hr; }

    hr = IMMDevice_Activate(g->device, &IID_IAudioEndpointVolume,
                            CLSCTX_INPROC_SERVER, NULL, (void **)&g->endpoint);
    if (FAILED(hr)) { g->endpoint = NULL; return hr; }

    IAudioEndpointVolume_RegisterControlChangeNotify(g->endpoint, &g->vol_cb_iface);

    g->device_name[0] = 0;
    if (SUCCEEDED(IMMDevice_OpenPropertyStore(g->device, STGM_READ, &props)))
    {
        ZeroMemory(&pv, sizeof(pv));
        if (SUCCEEDED(IPropertyStore_GetValue(props, &PKEY_Device_FriendlyName, &pv))
            && pv.vt == VT_LPWSTR && pv.pwszVal)
        {
            lstrcpynW(g->device_name, pv.pwszVal, ARRAY_SIZE(g->device_name));
            CoTaskMemFree(pv.pwszVal);
        }
        IPropertyStore_Release(props);
    }
    return S_OK;
}

static void unbind_endpoint(TRAY_GLOBALS *g)
{
    if (g->endpoint)
    {
        IAudioEndpointVolume_UnregisterControlChangeNotify(g->endpoint, &g->vol_cb_iface);
        IAudioEndpointVolume_Release(g->endpoint);
        g->endpoint = NULL;
    }
    if (g->device)
    {
        IMMDevice_Release(g->device);
        g->device = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Launch helpers                                                     */
/* ------------------------------------------------------------------ */

static void launch(const WCHAR *file, const WCHAR *params)
{
    ShellExecuteW(NULL, L"open", file, params, NULL, SW_SHOWNORMAL);
}

static void show_menu(TRAY_GLOBALS *g)
{
    POINT pt;
    HMENU sub;

    if (!g->hMenu)
        g->hMenu = LoadMenuW(g->hInstance, MAKEINTRESOURCEW(IDR_TRAYMENU));
    if (!g->hMenu) return;

    sub = GetSubMenu(g->hMenu, 0);
    GetCursorPos(&pt);

    /* The tray sits at the bottom edge, so the menu grows upward. The
     * foreground/WM_NULL dance is the documented way to make a popup from a
     * non-foreground tray owner dismiss correctly on an outside click. */
    SetForegroundWindow(g->hMainWnd);
    TrackPopupMenu(sub, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                   pt.x, pt.y, 0, g->hMainWnd, NULL);
    PostMessageW(g->hMainWnd, WM_NULL, 0, 0);
}

static void handle_command(WORD id)
{
    switch (id)
    {
    case IDM_MIXER:     launch(L"sndvol.exe", NULL);                break;
    case IDM_PLAYBACK:  launch(L"control.exe", L"mmsys.cpl,,0");    break;
    case IDM_RECORDING: launch(L"control.exe", L"mmsys.cpl,,1");    break;
    case IDM_SOUNDS:    launch(L"control.exe", L"mmsys.cpl,,2");    break;
    }
}

/* ------------------------------------------------------------------ */
/* Window procedure                                                   */
/* ------------------------------------------------------------------ */

static LRESULT WINAPI tray_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    TRAY_GLOBALS *g = &Globals;

    if (msg == g->msgTaskbarCreated)
    {
        /* systray host (re)started — re-add and resync. */
        add_tray(g);
        refresh_tray(g);
        return 0;
    }

    switch (msg)
    {
    case WM_APP_TRAYICON:
        switch (LOWORD(lp))
        {
        case WM_LBUTTONUP:
            launch(L"sndvol.exe", L"/flyout");
            break;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            show_menu(g);
            break;
        }
        return 0;

    case WM_APP_VOLCHANGED:
        refresh_tray(g);
        return 0;

    case WM_APP_REBIND:
        unbind_endpoint(g);
        bind_endpoint(g);
        refresh_tray(g);
        return 0;

    case WM_COMMAND:
        handle_command(LOWORD(wp));
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g->nid);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

int PASCAL wWinMain(HINSTANCE hInstance, HINSTANCE prev, LPWSTR cmdline, int show)
{
    WNDCLASSW wc;
    MSG msg;
    HANDLE mutex;

    Globals.hInstance = hInstance;

    /* Single instance: the shell may try to launch us more than once. */
    mutex = CreateMutexW(NULL, TRUE, L"YetiOS_AudioTray_Single");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(mutex);
        return 0;
    }

    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
    {
        if (mutex) CloseHandle(mutex);
        return 1;
    }

    Globals.vol_cb_iface.lpVtbl = &vol_cb_vtbl;
    Globals.notif_iface.lpVtbl  = &notif_vtbl;
    Globals.vol_cb_ref = 1;
    Globals.notif_ref  = 1;
    Globals.msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = tray_wndproc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"YetiOSAudioTray";
    if (!RegisterClassW(&wc)) goto done;

    /* Hidden owner window — never shown, only hosts the tray icon + menu. */
    Globals.hMainWnd = CreateWindowExW(0, L"YetiOSAudioTray", NULL, WS_POPUP,
                                       0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    if (!Globals.hMainWnd) goto done;

    if (SUCCEEDED(CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL,
                                   CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator,
                                   (void **)&Globals.enumerator)))
    {
        IMMDeviceEnumerator_RegisterEndpointNotificationCallback(Globals.enumerator,
                                                                 &Globals.notif_iface);
        bind_endpoint(&Globals);
    }

    add_tray(&Globals);
    refresh_tray(&Globals);

    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    unbind_endpoint(&Globals);
    if (Globals.enumerator)
    {
        IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(Globals.enumerator,
                                                                   &Globals.notif_iface);
        IMMDeviceEnumerator_Release(Globals.enumerator);
    }
    if (Globals.hIcon) DestroyIcon(Globals.hIcon);
    if (Globals.hMenu) DestroyMenu(Globals.hMenu);

done:
    CoUninitialize();
    if (mutex) { ReleaseMutex(mutex); CloseHandle(mutex); }
    return 0;
}
