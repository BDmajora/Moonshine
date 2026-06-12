/*
 * Copyright 2010 Maarten Lankhorst for CodeWeavers
 * Copyright 2025 YetiOS contributors (endpoint volume backend wiring)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS

#include <stdarg.h>
#include <math.h>

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winreg.h"
#include "wine/debug.h"

#include "ole2.h"
#include "mmdeviceapi.h"
#include "mmsystem.h"
#include "dsound.h"
#include "audioclient.h"
#include "endpointvolume.h"
#include "audiopolicy.h"
#include "spatialaudioclient.h"

#include "mmdevapi_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmdevapi);

struct notify_node {
    struct list entry;
    IAudioEndpointVolumeCallback *cb;
};

typedef struct AEVImpl {
    IAudioEndpointVolumeEx IAudioEndpointVolumeEx_iface;
    LONG ref;

    /* Backend endpoint this object controls. device may be NULL, meaning
     * "the default endpoint of this flow" — the driver resolves that. */
    char *device;
    EDataFlow flow;

    /* Registered change-notification callbacks. */
    struct list notifications;
    CRITICAL_SECTION lock;
} AEVImpl;

static inline AEVImpl *impl_from_IAudioEndpointVolumeEx(IAudioEndpointVolumeEx *iface)
{
    return CONTAINING_RECORD(iface, AEVImpl, IAudioEndpointVolumeEx_iface);
}

/* ------------------------------------------------------------------ *
 *  scalar (0..1, linear) <-> dB helpers                               *
 *  Matches the -100..0 dB range reported by GetVolumeRange.           *
 * ------------------------------------------------------------------ */

static float scalar_to_db(float s)
{
    float db;
    if (s <= 0.0f) return -100.0f;
    db = 20.0f * log10f(s);
    if (db < -100.0f) db = -100.0f;
    if (db > 0.0f)    db = 0.0f;
    return db;
}

static float db_to_scalar(float db)
{
    if (db <= -100.0f) return 0.0f;
    if (db > 0.0f) db = 0.0f;
    return powf(10.0f, db / 20.0f);
}

/* ------------------------------------------------------------------ *
 *  Backend round-trips                                                *
 * ------------------------------------------------------------------ */

static HRESULT backend_set_volume(AEVImpl *This, float level, int mute)
{
    struct set_endpoint_volume_params params;
    params.device = This->device;
    params.flow   = This->flow;
    params.level  = level;   /* < 0 leaves volume unchanged */
    params.mute   = mute;    /* -1 leaves mute unchanged    */
    wine_unix_call(set_endpoint_volume, &params);
    return params.result;
}

static HRESULT backend_get_volume(AEVImpl *This, float *level, int *mute)
{
    struct get_endpoint_volume_params params;
    params.device = This->device;
    params.flow   = This->flow;
    params.level  = level;
    params.mute   = mute;
    wine_unix_call(get_endpoint_volume, &params);
    return params.result;
}

/* Read current backend state and broadcast it to all registered callbacks. */
static void AEV_notify(AEVImpl *This, const GUID *ctx)
{
    AUDIO_VOLUME_NOTIFICATION_DATA *data;
    struct notify_node *node;
    float scalar = 1.0f;
    int mute = 0;

    backend_get_volume(This, &scalar, &mute);

    data = malloc(FIELD_OFFSET(AUDIO_VOLUME_NOTIFICATION_DATA, afChannelVolumes[1]));
    if (!data)
        return;

    if (ctx) data->guidEventContext = *ctx;
    else     memset(&data->guidEventContext, 0, sizeof(GUID));
    data->bMuted          = mute ? TRUE : FALSE;
    data->fMasterVolume   = scalar;
    data->nChannels       = 1;
    data->afChannelVolumes[0] = scalar;

    EnterCriticalSection(&This->lock);
    LIST_FOR_EACH_ENTRY(node, &This->notifications, struct notify_node, entry)
        IAudioEndpointVolumeCallback_OnNotify(node->cb, data);
    LeaveCriticalSection(&This->lock);

    free(data);
}

static void AudioEndpointVolume_Destroy(AEVImpl *This)
{
    struct notify_node *node, *next;

    EnterCriticalSection(&This->lock);
    LIST_FOR_EACH_ENTRY_SAFE(node, next, &This->notifications, struct notify_node, entry) {
        list_remove(&node->entry);
        IAudioEndpointVolumeCallback_Release(node->cb);
        free(node);
    }
    LeaveCriticalSection(&This->lock);

    DeleteCriticalSection(&This->lock);
    free(This->device);
    free(This);
}

static HRESULT WINAPI AEV_QueryInterface(IAudioEndpointVolumeEx *iface, REFIID riid, void **ppv)
{
    AEVImpl *This = impl_from_IAudioEndpointVolumeEx(iface);
    TRACE("(%p)->(%s,%p)\n", This, debugstr_guid(riid), ppv);
    if (!ppv)
        return E_POINTER;
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IAudioEndpointVolume) ||
        IsEqualIID(riid, &IID_IAudioEndpointVolumeEx)) {
        *ppv = &This->IAudioEndpointVolumeEx_iface;
    }
    else
        return E_NOINTERFACE;
    IUnknown_AddRef((IUnknown *)*ppv);
    return S_OK;
}

static ULONG WINAPI AEV_AddRef(IAudioEndpointVolumeEx *iface)
{
    AEVImpl *This = impl_from_IAudioEndpointVolumeEx(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p) new ref %lu\n", This, ref);
    return ref;
}

static ULONG WINAPI AEV_Release(IAudioEndpointVolumeEx *iface)
{
    AEVImpl *This = impl_from_IAudioEndpointVolumeEx(iface);
    ULONG ref = InterlockedDecrement(&This->ref);
    TRACE("(%p) new ref %lu\n", This, ref);
    if (!ref)
        AudioEndpointVolume_Destroy(This);
    return ref;
}

static HRESULT WINAPI AEV_RegisterControlChangeNotify(IAudioEndpointVolumeEx *iface, IAudioEndpointVolumeCallback *notify)
{
    AEVImpl *This = impl_from_IAudioEndpointVolumeEx(iface);
    struct notify_node *node;

    TRACE("(%p)->(%p)\n", This, notify);
    if (!notify)
        return E_POINTER;

    if (!(node = malloc(sizeof(*node))))
        return E_OUTOFMEMORY;
    node->cb = notify;
    IAudioEndpointVolumeCallback_AddRef(notify);

    EnterCriticalSection(&This->lock);
    list_add_tail(&This->notifications, &node->entry);
    LeaveCriticalSection(&This->lock);
    return S_OK;
}

static HRESULT WINAPI AEV_UnregisterControlChangeNotify(IAudioEndpointVolumeEx *iface, IAudioEndpointVolumeCallback *notify)
{
    AEVImpl *This = impl_from_IAudioEndpointVolumeEx(iface);
    struct notify_node *node;
    HRESULT hr = E_NOTFOUND;

    TRACE("(%p)->(%p)\n", This, notify);
    if (!notify)
        return E_POINTER;

    EnterCriticalSection(&This->lock);
    LIST_FOR_EACH_ENTRY(node, &This->notifications, struct notify_node, entry) {
        if (node->cb == notify) {
            list_remove(&node->entry);
            IAudioEndpointVolumeCallback_Release(node->cb);
            free(node);
            hr = S_OK;
            break;
        }
    }
    LeaveCriticalSection(&This->lock);
    return hr;
}

static HRESULT WINAPI AEV_GetChannelCount(IAudioEndpointVolumeEx *iface, UINT *count)
{
    TRACE("(%p)->(%p)\n", iface, count);
    if (!count)
        return E_POINTER;
    /* Master-only endpoint volume for now. */
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI AEV_SetMasterVolumeLevel(IAudioEndpointVolumeEx *iface, float leveldb, const GUID *ctx)
{
    AEVImpl *This = impl_from_IAudioEndpointVolumeEx(iface);
    HRESULT hr;

    TRACE("(%p)->(%f,%s)\n", iface, leveldb, debugstr_guid(ctx));

    if (leveldb < -100.f || leveldb > 0.f)
        return E_INVALIDARG;

    hr = backend_set_volume(This, db_to_scalar(leveldb), -1);
    if (SUCCEEDED(hr))
        AEV_notify(This, ctx);
    return hr;
}

static HRESULT WINAPI AEV_SetMasterVolumeLevelScalar(IAudioEndpointVolumeEx *iface, float level, const GUID *ctx)
{
    AEVImpl *This = impl_from_IAudioEndpointVolumeEx(iface);
    HRESULT hr;

    TRACE("(%p)->(%f,%s)\n", iface, level, debugstr_guid(ctx));

    if (level < 0.f || level > 1.f)
        return E_INVALIDARG;

    hr = backend_set_volume(This, level, -1);
    if (SUCCEEDED(hr))
        AEV_notify(This, ctx);
    return hr;
}

static HRESULT WINAPI AEV_GetMasterVolumeLevel(IAudioEndpointVolumeEx *iface, float *leveldb)
{
    AEVImpl *This = impl_from_IAudioEndpointVolumeEx(iface);
    float scalar = 1.0f;

    TRACE("(%p)->(%p)\n", iface, leveldb);
    if (!leveldb)
        return E_POINTER;

    backend_get_volume(This, &scalar, NULL);
    *leveldb = scalar_to_db(scalar);
    return S_OK;
}

static HRESULT WINAPI AEV_GetMasterVolumeLevelScalar(IAudioEndpointVolumeEx *iface, float *level)
{
    AEVImpl *This = impl_from_IAudioEndpointVolumeEx(iface);

    TRACE("(%p)->(%p)\n", iface, level);
    if (!level)
        return E_POINTER;

    *level = 1.0f;
    backend_get_volume(This, level, NULL);
    return S_OK;
}

static HRESULT WINAPI AEV_SetChannelVolumeLevel(IAudioEndpointVolumeEx *iface, UINT chan, float leveldb, const GUID *ctx)
{
    TRACE("(%p)->(%u,%f,%s)\n", iface, chan, leveldb, debugstr_guid(ctx));
    FIXME("per-channel endpoint volume not supported\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI AEV_SetChannelVolumeLevelScalar(IAudioEndpointVolumeEx *iface, UINT chan, float level, const GUID *ctx)
{
    TRACE("(%p)->(%u,%f,%s)\n", iface, chan, level, debugstr_guid(ctx));
    FIXME("per-channel endpoint volume not supported\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI AEV_GetChannelVolumeLevel(IAudioEndpointVolumeEx *iface, UINT chan, float *leveldb)
{
    TRACE("(%p)->(%u,%p)\n", iface, chan, leveldb);
    if (!leveldb)
        return E_POINTER;
    FIXME("per-channel endpoint volume not supported\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI AEV_GetChannelVolumeLevelScalar(IAudioEndpointVolumeEx *iface, UINT chan, float *level)
{
    TRACE("(%p)->(%u,%p)\n", iface, chan, level);
    if (!level)
        return E_POINTER;
    FIXME("per-channel endpoint volume not supported\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI AEV_SetMute(IAudioEndpointVolumeEx *iface, BOOL mute, const GUID *ctx)
{
    AEVImpl *This = impl_from_IAudioEndpointVolumeEx(iface);
    int prev = 0;
    HRESULT hr;

    TRACE("(%p)->(%u,%s)\n", iface, mute, debugstr_guid(ctx));

    backend_get_volume(This, NULL, &prev);

    hr = backend_set_volume(This, -1.0f, mute ? 1 : 0);
    if (FAILED(hr))
        return hr;

    AEV_notify(This, ctx);

    /* S_OK if the state changed, S_FALSE if it was already in that state. */
    return (!!prev == !!mute) ? S_FALSE : S_OK;
}

static HRESULT WINAPI AEV_GetMute(IAudioEndpointVolumeEx *iface, BOOL *mute)
{
    AEVImpl *This = impl_from_IAudioEndpointVolumeEx(iface);
    int m = 0;

    TRACE("(%p)->(%p)\n", iface, mute);
    if (!mute)
        return E_POINTER;

    backend_get_volume(This, NULL, &m);
    *mute = m ? TRUE : FALSE;
    return S_OK;
}

static HRESULT WINAPI AEV_GetVolumeStepInfo(IAudioEndpointVolumeEx *iface, UINT *stepsize, UINT *stepcount)
{
    TRACE("(%p)->(%p,%p)\n", iface, stepsize, stepcount);
    if (!stepsize && !stepcount)
        return E_POINTER;
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI AEV_VolumeStepUp(IAudioEndpointVolumeEx *iface, const GUID *ctx)
{
    TRACE("(%p)->(%s)\n", iface, debugstr_guid(ctx));
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI AEV_VolumeStepDown(IAudioEndpointVolumeEx *iface, const GUID *ctx)
{
    TRACE("(%p)->(%s)\n", iface, debugstr_guid(ctx));
    FIXME("stub\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI AEV_QueryHardwareSupport(IAudioEndpointVolumeEx *iface, DWORD *mask)
{
    TRACE("(%p)->(%p)\n", iface, mask);
    if (!mask)
        return E_POINTER;
    /* Volume + mute handled in software by the backend, not hardware. */
    *mask = 0;
    return S_OK;
}

static HRESULT WINAPI AEV_GetVolumeRange(IAudioEndpointVolumeEx *iface, float *mindb, float *maxdb, float *inc)
{
    AEVImpl *This = impl_from_IAudioEndpointVolumeEx(iface);
    struct get_endpoint_volume_range_params params;

    TRACE("(%p)->(%p,%p,%p)\n", iface, mindb, maxdb, inc);

    if (!mindb || !maxdb || !inc)
        return E_POINTER;

    params.device = This->device;
    params.flow   = This->flow;
    params.min_db = mindb;
    params.max_db = maxdb;
    params.inc_db = inc;
    wine_unix_call(get_endpoint_volume_range, &params);

    if (FAILED(params.result)) {
        *mindb = -100.f;
        *maxdb = 0.f;
        *inc   = 1.f;
    }
    return S_OK;
}

static HRESULT WINAPI AEV_GetVolumeRangeChannel(IAudioEndpointVolumeEx *iface, UINT chan, float *mindb, float *maxdb, float *inc)
{
    TRACE("(%p)->(%u,%p,%p,%p)\n", iface, chan, mindb, maxdb, inc);
    if (!mindb || !maxdb || !inc)
        return E_POINTER;
    FIXME("per-channel range not supported\n");
    return E_NOTIMPL;
}

static const IAudioEndpointVolumeExVtbl AEVImpl_Vtbl = {
    AEV_QueryInterface,
    AEV_AddRef,
    AEV_Release,
    AEV_RegisterControlChangeNotify,
    AEV_UnregisterControlChangeNotify,
    AEV_GetChannelCount,
    AEV_SetMasterVolumeLevel,
    AEV_SetMasterVolumeLevelScalar,
    AEV_GetMasterVolumeLevel,
    AEV_GetMasterVolumeLevelScalar,
    AEV_SetChannelVolumeLevel,
    AEV_SetChannelVolumeLevelScalar,
    AEV_GetChannelVolumeLevel,
    AEV_GetChannelVolumeLevelScalar,
    AEV_SetMute,
    AEV_GetMute,
    AEV_GetVolumeStepInfo,
    AEV_VolumeStepUp,
    AEV_VolumeStepDown,
    AEV_QueryHardwareSupport,
    AEV_GetVolumeRange,
    AEV_GetVolumeRangeChannel
};

HRESULT AudioEndpointVolume_Create(MMDevice *parent, IAudioEndpointVolumeEx **ppv)
{
    AEVImpl *This;

    *ppv = NULL;
    This = calloc(1, sizeof(*This));
    if (!This)
        return E_OUTOFMEMORY;
    This->IAudioEndpointVolumeEx_iface.lpVtbl = &AEVImpl_Vtbl;
    This->ref = 1;
    This->flow = parent->flow;
    list_init(&This->notifications);
    InitializeCriticalSection(&This->lock);

    /* Resolve the backend device name for this endpoint. On failure we leave
     * device NULL, and the driver operates on the default endpoint. */
    if (!get_device_name_from_guid(&parent->devguid, &This->device, &This->flow))
        This->device = NULL;

    *ppv = &This->IAudioEndpointVolumeEx_iface;
    return S_OK;
}