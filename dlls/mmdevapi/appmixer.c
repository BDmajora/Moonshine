/* appmixer.c — IAppMixer: per-application playback mixer.
 * Forwards to the audio driver's coalesced playback-stream graph. */

#define COBJMACROS

#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <winternl.h>

#include <wine/debug.h>
#include <wine/unixlib.h>

#include "appmixer.h"
#include "mmdevapi_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmdevapi);

const GUID IID_IAppMixer =
    { 0x7e71a000, 0x9c0f, 0x4d2e, { 0xb3, 0x55, 0x2a, 0x11, 0x76, 0x55, 0x4f, 0x01 } };

struct app_mixer
{
    IAppMixer IAppMixer_iface;
    LONG ref;
};

static inline struct app_mixer *impl_from_IAppMixer(IAppMixer *iface)
{
    return CONTAINING_RECORD(iface, struct app_mixer, IAppMixer_iface);
}

static HRESULT STDMETHODCALLTYPE am_QueryInterface(IAppMixer *iface, REFIID riid, void **ppv)
{
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IAppMixer))
        *ppv = iface;
    else { *ppv = NULL; return E_NOINTERFACE; }
    IUnknown_AddRef((IUnknown *)*ppv);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE am_AddRef(IAppMixer *iface)
{
    struct app_mixer *This = impl_from_IAppMixer(iface);
    return InterlockedIncrement(&This->ref);
}

static ULONG STDMETHODCALLTYPE am_Release(IAppMixer *iface)
{
    struct app_mixer *This = impl_from_IAppMixer(iface);
    ULONG ref = InterlockedDecrement(&This->ref);
    if (!ref) free(This);
    return ref;
}

static HRESULT STDMETHODCALLTYPE am_GetStreams(IAppMixer *iface, UINT max,
                                               appmixer_entry *entries, UINT *count)
{
    struct get_playback_streams_params params;
    struct stream_info *buf;
    UINT i;

    if (!entries || !count) return E_POINTER;
    *count = 0;

    if (!(buf = calloc(max ? max : 1, sizeof(*buf))))
        return E_OUTOFMEMORY;

    params.streams = buf;
    params.max = max;
    wine_unix_call(get_playback_streams, &params);
    if (FAILED(params.result)) { free(buf); return params.result; }

    *count = params.count < max ? params.count : max;
    for (i = 0; i < *count; i++) {
        entries[i].id     = buf[i].id;
        entries[i].volume = buf[i].volume;
        entries[i].mute   = buf[i].mute;
        lstrcpynW(entries[i].name, buf[i].name, APPMIXER_NAME_LEN);
    }
    free(buf);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE am_GetVolume(IAppMixer *iface, UINT id,
                                              float *volume, BOOL *mute)
{
    struct get_stream_volume_params params;
    int m = 0;
    params.id = id;
    params.level = volume;
    params.mute = mute ? &m : NULL;
    wine_unix_call(get_stream_volume, &params);
    if (mute) *mute = m ? TRUE : FALSE;
    return params.result;
}

static HRESULT STDMETHODCALLTYPE am_SetVolume(IAppMixer *iface, UINT id, float volume)
{
    struct set_stream_volume_params params;
    if (volume < 0.f || volume > 1.f) return E_INVALIDARG;
    params.id = id;
    params.level = volume;
    params.mute = -1;
    wine_unix_call(set_stream_volume, &params);
    return params.result;
}

static HRESULT STDMETHODCALLTYPE am_SetMute(IAppMixer *iface, UINT id, BOOL mute)
{
    struct set_stream_volume_params params;
    params.id = id;
    params.level = -1.0f;
    params.mute = mute ? 1 : 0;
    wine_unix_call(set_stream_volume, &params);
    return params.result;
}

static const IAppMixerVtbl appmixer_vtbl =
{
    am_QueryInterface,
    am_AddRef,
    am_Release,
    am_GetStreams,
    am_GetVolume,
    am_SetVolume,
    am_SetMute,
};

HRESULT AppMixer_Create(IMMDevice *device, void **ppv)
{
    struct app_mixer *This;
    (void)device;   /* the graph is global; arg kept for Activate symmetry */
    if (!(This = calloc(1, sizeof(*This))))
        return E_OUTOFMEMORY;
    This->IAppMixer_iface.lpVtbl = &appmixer_vtbl;
    This->ref = 1;
    *ppv = &This->IAppMixer_iface;
    return S_OK;
}