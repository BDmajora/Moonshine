/* sndvol.exe — per-application source enumeration.
 *
 * The Applications columns reflect the system playback graph, coalesced to
 * one row per application (an app that opens several streams shows a single
 * slider). Data comes from mmdevapi's IAppMixer, which surfaces every output
 * stream — Windows-side or native — keyed by application.
 *
 * Each row's slider/mute is driven through a small ISimpleAudioVolume that
 * forwards to IAppMixer for that application group, so the window code is
 * unchanged from the session-based version.
 */

#include "sndvol_private.h"
#include "appmixer.h"

WINE_DEFAULT_DEBUG_CHANNEL(sndvol);

const GUID IID_IAppMixer =
    { 0x7e71a000, 0x9c0f, 0x4d2e, { 0xb3, 0x55, 0x2a, 0x11, 0x76, 0x55, 0x4f, 0x01 } };

/* ------------------------------------------------------------------ */
/* Per-group ISimpleAudioVolume backed by IAppMixer                   */
/* ------------------------------------------------------------------ */

struct group_vol
{
    ISimpleAudioVolume ISimpleAudioVolume_iface;
    LONG ref;
    IAppMixer *mixer;          /* AddRef'd */
    UINT id;
    float fallback_vol;        /* used if a live read fails */
    BOOL  fallback_mute;
};

static inline struct group_vol *impl_from_ISimpleAudioVolume(ISimpleAudioVolume *iface)
{
    return CONTAINING_RECORD(iface, struct group_vol, ISimpleAudioVolume_iface);
}

static HRESULT WINAPI gv_QueryInterface(ISimpleAudioVolume *iface, REFIID riid, void **ppv)
{
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ISimpleAudioVolume))
        *ppv = iface;
    else { *ppv = NULL; return E_NOINTERFACE; }
    IUnknown_AddRef((IUnknown *)*ppv);
    return S_OK;
}

static ULONG WINAPI gv_AddRef(ISimpleAudioVolume *iface)
{
    struct group_vol *v = impl_from_ISimpleAudioVolume(iface);
    return InterlockedIncrement(&v->ref);
}

static ULONG WINAPI gv_Release(ISimpleAudioVolume *iface)
{
    struct group_vol *v = impl_from_ISimpleAudioVolume(iface);
    ULONG ref = InterlockedDecrement(&v->ref);
    if (!ref) {
        if (v->mixer) IAppMixer_Release(v->mixer);
        free(v);
    }
    return ref;
}

static HRESULT WINAPI gv_SetMasterVolume(ISimpleAudioVolume *iface, float level, const GUID *ctx)
{
    struct group_vol *v = impl_from_ISimpleAudioVolume(iface);
    if (level < 0.f || level > 1.f) return E_INVALIDARG;
    v->fallback_vol = level;
    return IAppMixer_SetVolume(v->mixer, v->id, level);
}

static HRESULT WINAPI gv_GetMasterVolume(ISimpleAudioVolume *iface, float *level)
{
    struct group_vol *v = impl_from_ISimpleAudioVolume(iface);
    float cur;
    if (!level) return E_POINTER;
    if (SUCCEEDED(IAppMixer_GetVolume(v->mixer, v->id, &cur, NULL)))
        *level = v->fallback_vol = cur;
    else
        *level = v->fallback_vol;
    return S_OK;
}

static HRESULT WINAPI gv_SetMute(ISimpleAudioVolume *iface, BOOL mute, const GUID *ctx)
{
    struct group_vol *v = impl_from_ISimpleAudioVolume(iface);
    v->fallback_mute = mute;
    return IAppMixer_SetMute(v->mixer, v->id, mute);
}

static HRESULT WINAPI gv_GetMute(ISimpleAudioVolume *iface, BOOL *mute)
{
    struct group_vol *v = impl_from_ISimpleAudioVolume(iface);
    BOOL cur;
    if (!mute) return E_POINTER;
    if (SUCCEEDED(IAppMixer_GetVolume(v->mixer, v->id, NULL, &cur)))
        *mute = v->fallback_mute = cur;
    else
        *mute = v->fallback_mute;
    return S_OK;
}

static const ISimpleAudioVolumeVtbl group_vol_vtbl =
{
    gv_QueryInterface,
    gv_AddRef,
    gv_Release,
    gv_SetMasterVolume,
    gv_GetMasterVolume,
    gv_SetMute,
    gv_GetMute,
};

static ISimpleAudioVolume *group_vol_create(IAppMixer *mixer, UINT id,
                                            float vol, BOOL mute)
{
    struct group_vol *v = calloc(1, sizeof(*v));
    if (!v) return NULL;
    v->ISimpleAudioVolume_iface.lpVtbl = &group_vol_vtbl;
    v->ref = 1;
    v->mixer = mixer;
    IAppMixer_AddRef(mixer);
    v->id = id;
    v->fallback_vol = vol;
    v->fallback_mute = mute;
    return &v->ISimpleAudioVolume_iface;
}

/* ------------------------------------------------------------------ */
/* Snapshot                                                           */
/* ------------------------------------------------------------------ */

int snapshot_sessions(IMMDevice *dev, struct session_info **out)
{
    IAppMixer *mixer = NULL;
    appmixer_entry entries[MAX_SNAP];
    struct session_info *sessions;
    UINT count = 0, i;
    HRESULT hr;

    *out = NULL;

    hr = IMMDevice_Activate(dev, &IID_IAppMixer, CLSCTX_INPROC_SERVER,
                            NULL, (void **)&mixer);
    if (FAILED(hr)) {
        WARN("IAppMixer activation failed: %08lx\n", hr);
        return -1;
    }

    hr = IAppMixer_GetStreams(mixer, MAX_SNAP, entries, &count);
    if (FAILED(hr)) {
        IAppMixer_Release(mixer);
        return -1;
    }
    if (!count) {
        IAppMixer_Release(mixer);
        return 0;
    }

    sessions = calloc(count, sizeof(*sessions));
    if (!sessions) {
        IAppMixer_Release(mixer);
        return -1;
    }

    for (i = 0; i < count; i++) {
        sessions[i].id = entries[i].id;
        lstrcpynW(sessions[i].label, entries[i].name, ARRAY_SIZE(sessions[i].label));
        sessions[i].volume = group_vol_create(mixer, entries[i].id,
                                               entries[i].volume,
                                               entries[i].mute ? TRUE : FALSE);
    }

    IAppMixer_Release(mixer);   /* each row holds its own ref */
    *out = sessions;
    return (int)count;
}

void free_sessions(struct session_info *sessions, int count)
{
    int i;
    if (!sessions) return;
    for (i = 0; i < count; i++)
        if (sessions[i].volume) ISimpleAudioVolume_Release(sessions[i].volume);
    free(sessions);
}

BOOL sessions_equal(const struct session_info *a, int na,
                    const struct session_info *b, int nb)
{
    int i;
    if (na != nb) return FALSE;
    for (i = 0; i < na; i++)
        if (a[i].id != b[i].id) return FALSE;
    return TRUE;
}