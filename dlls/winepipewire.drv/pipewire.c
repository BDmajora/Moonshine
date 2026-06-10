/*
 * PipeWire WASAPI backend for YetiOS (winepipewire.drv)
 *
 * Faithful port of winepulse.drv (pulse.c) to libpipewire-0.3.
 * The ring-buffer bookkeeping is carried over from pulse.c verbatim
 * (it is backend-agnostic); only the I/O layer differs:
 *
 *   pulse                          ->  pipewire
 *   ------------------------------     ------------------------------
 *   pa_threaded mainloop model     ->  pw_thread_loop
 *   pa_context / pa_mainloop       ->  pw_context / pw_core
 *   pa_context_get_sink_info_list  ->  pw_registry global enumeration
 *   pa_sample_spec / pa_channel_map->  spa_audio_info_raw (SPA pod)
 *   pa_stream_write (push)         ->  on_process pull (dequeue/queue buffer)
 *   pa_stream_peek/drop            ->  on_process push into packet ring
 *
 * Copyright 2025 YetiOS contributors
 * Derived from winepulse.drv:
 *   Copyright 2011-2012 Maarten Lankhorst
 *   Copyright 2010-2011 Maarten Lankhorst for CodeWeavers
 *   Copyright 2011 Andrew Eikum for CodeWeavers
 *   Copyright 2022 Huw Davies
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
 */

#if 0
#pragma makedep unix
#endif

#include <stdarg.h>
#include <pthread.h>
#include <math.h>

#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/param/param.h>
#include <spa/pod/parser.h>
#include <spa/utils/result.h>
#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"

#include "mmdeviceapi.h"
#include "initguid.h"
#include "audioclient.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "wine/unixlib.h"

#include "../mmdevapi/unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(pipewire);

/* ------------------------------------------------------------------ *
 *  Sample format (replaces pa_sample_spec)                            *
 * ------------------------------------------------------------------ */

enum pw_fmt
{
    PW_FMT_INVALID = -1,
    PW_FMT_U8,
    PW_FMT_S16,
    PW_FMT_S24,        /* 3-byte packed */
    PW_FMT_S24_32,     /* 24 valid bits in a 32-bit container */
    PW_FMT_S32,
    PW_FMT_F32,
};

struct sample_spec
{
    enum pw_fmt format;
    UINT32 rate;
    UINT32 channels;
};

#define PW_CHANNELS_MAX SPA_AUDIO_MAX_CHANNELS

static UINT bytes_per_sample(enum pw_fmt f)
{
    switch (f)
    {
    case PW_FMT_U8:     return 1;
    case PW_FMT_S16:    return 2;
    case PW_FMT_S24:    return 3;
    case PW_FMT_S24_32: return 4;
    case PW_FMT_S32:    return 4;
    case PW_FMT_F32:    return 4;
    default:            return 0;
    }
}

static UINT frame_size(const struct sample_spec *ss)
{
    return bytes_per_sample(ss->format) * ss->channels;
}

static enum spa_audio_format spa_format_from_fmt(enum pw_fmt f)
{
    switch (f)
    {
    case PW_FMT_U8:     return SPA_AUDIO_FORMAT_U8;
    case PW_FMT_S16:    return SPA_AUDIO_FORMAT_S16_LE;
    case PW_FMT_S24:    return SPA_AUDIO_FORMAT_S24_LE;
    case PW_FMT_S24_32: return SPA_AUDIO_FORMAT_S24_32_LE;
    case PW_FMT_S32:    return SPA_AUDIO_FORMAT_S32_LE;
    case PW_FMT_F32:    return SPA_AUDIO_FORMAT_F32_LE;
    default:            return SPA_AUDIO_FORMAT_UNKNOWN;
    }
}

static void silence_buffer(enum pw_fmt format, BYTE *buffer, UINT32 bytes)
{
    memset(buffer, format == PW_FMT_U8 ? 0x80 : 0, bytes);
}

/* ------------------------------------------------------------------ *
 *  Per-stream state                                                   *
 * ------------------------------------------------------------------ */

typedef struct _ACPacket
{
    struct list entry;
    UINT64 qpcpos;
    BYTE *data;
    UINT32 discont;
} ACPacket;

struct pw_audio_stream
{
    EDataFlow dataflow;

    struct pw_stream *stream;
    struct spa_hook stream_listener;
    struct sample_spec ss;
    UINT channel_mask;

    DWORD flags;
    AUDCLNT_SHAREMODE share;
    HANDLE event;
    float vol[PW_CHANNELS_MAX];

    REFERENCE_TIME def_period;
    REFERENCE_TIME duration;

    INT32 locked;
    BOOL started;
    SIZE_T bufsize_frames, real_bufsize_bytes, period_bytes;
    SIZE_T peek_ofs, read_offs_bytes, lcl_offs_bytes, pw_offs_bytes;
    SIZE_T tmp_buffer_bytes, held_bytes, peek_len, peek_buffer_len, pw_held_bytes;
    BYTE *local_buffer, *tmp_buffer, *peek_buffer;
    void *locked_ptr;
    BOOL please_quit, just_started, just_underran;
    UINT64 mmdev_period_usec;

    INT64 clock_lastpos, clock_written;

    struct list packet_free_head;
    struct list packet_filled_head;
};

/* ------------------------------------------------------------------ *
 *  Physical device descriptor (replaces PhysDevice)                   *
 * ------------------------------------------------------------------ */

enum phys_device_bus_type
{
    phys_device_bus_invalid = -1,
    phys_device_bus_pci,
    phys_device_bus_usb
};

typedef struct _PhysDevice
{
    struct list entry;
    WCHAR *name;
    enum phys_device_bus_type bus_type;
    USHORT vendor_id, product_id;
    EndpointFormFactor form;
    UINT channel_mask;
    UINT index;                 /* PipeWire object.serial / node id */
    UINT node_id;               /* PipeWire registry global id (for pw_registry_bind) */
    REFERENCE_TIME min_period, def_period;
    WAVEFORMATEXTENSIBLE fmt;
    char pw_name[0];            /* PipeWire node.name */
} PhysDevice;

/* ------------------------------------------------------------------ *
 *  Globals                                                            *
 * ------------------------------------------------------------------ */

static struct pw_thread_loop *pw_loop;
static struct pw_context     *pw_ctx;
static struct pw_core        *pw_core;
static struct pw_registry    *pw_reg;
static struct spa_hook        reg_listener;
static struct spa_hook        core_listener;
static int                    core_sync_seq;
static BOOL                   core_sync_done;
static UINT                   g_metadata_id;   /* registry id of metadata.name=="default" */

static struct list g_phys_speakers = LIST_INIT(g_phys_speakers);
static struct list g_phys_sources  = LIST_INIT(g_phys_sources);

static pthread_mutex_t loop_park_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  loop_park_cond  = PTHREAD_COND_INITIALIZER;
static BOOL            loop_park_quit;

static ULONG_PTR zero_bits = 0;

static NTSTATUS pw_not_implemented(void *args)
{
    return STATUS_SUCCESS;
}

/* pw_thread_loop owns a recursive lock; use it as the global lock,
 * exactly as pulse used pa_threaded_mainloop_lock(). */
static void pw_lock(void)   { if (pw_loop) pw_thread_loop_lock(pw_loop); }
static void pw_unlock(void) { if (pw_loop) pw_thread_loop_unlock(pw_loop); }

static struct pw_audio_stream *handle_get_stream(stream_handle h)
{
    return (struct pw_audio_stream *)(UINT_PTR)h;
}

static int muldiv(int a, int b, int c)
{
    LONGLONG ret;
    if (!c) return -1;
    if (c < 0) { a = -a; c = -c; }
    if ((a < 0 && b < 0) || (a >= 0 && b >= 0))
        ret = (((LONGLONG)a * b) + (c / 2)) / c;
    else
        ret = (((LONGLONG)a * b) - (c / 2)) / c;
    if (ret > 2147483647 || ret < -2147483647) return -1;
    return ret;
}

static void free_phys_device_lists(void)
{
    static struct list *const lists[] = { &g_phys_speakers, &g_phys_sources, NULL };
    struct list *const *list = lists;
    PhysDevice *dev, *dev_next;
    do {
        LIST_FOR_EACH_ENTRY_SAFE(dev, dev_next, *list, PhysDevice, entry) {
            free(dev->name);
            free(dev);
        }
    } while (*(++list));
}

/* ------------------------------------------------------------------ *
 *  Channel mask <-> SPA position                                      *
 * ------------------------------------------------------------------ */

static const enum spa_audio_channel pos_from_wfx[] = {
    SPA_AUDIO_CHANNEL_FL,  SPA_AUDIO_CHANNEL_FR,  SPA_AUDIO_CHANNEL_FC,
    SPA_AUDIO_CHANNEL_LFE, SPA_AUDIO_CHANNEL_RL,  SPA_AUDIO_CHANNEL_RR,
    SPA_AUDIO_CHANNEL_FLC, SPA_AUDIO_CHANNEL_FRC, SPA_AUDIO_CHANNEL_RC,
    SPA_AUDIO_CHANNEL_SL,  SPA_AUDIO_CHANNEL_SR,  SPA_AUDIO_CHANNEL_TC,
    SPA_AUDIO_CHANNEL_TFL, SPA_AUDIO_CHANNEL_TFC, SPA_AUDIO_CHANNEL_TFR,
    SPA_AUDIO_CHANNEL_TRL, SPA_AUDIO_CHANNEL_TRC, SPA_AUDIO_CHANNEL_TRR,
};

static UINT get_channel_mask(unsigned int channels)
{
    switch (channels) {
    case 0: return 0;
    case 1: return KSAUDIO_SPEAKER_MONO;
    case 2: return KSAUDIO_SPEAKER_STEREO;
    case 3: return KSAUDIO_SPEAKER_STEREO | SPEAKER_LOW_FREQUENCY;
    case 4: return KSAUDIO_SPEAKER_QUAD;
    case 5: return KSAUDIO_SPEAKER_QUAD | SPEAKER_LOW_FREQUENCY;
    case 6: return KSAUDIO_SPEAKER_5POINT1;
    case 7: return KSAUDIO_SPEAKER_5POINT1 | SPEAKER_BACK_CENTER;
    case 8: return KSAUDIO_SPEAKER_7POINT1_SURROUND;
    }
    FIXME("Unknown speaker configuration: %u\n", channels);
    return 0;
}

static void fill_spa_position(struct spa_audio_info_raw *info, UINT mask, UINT channels)
{
    unsigned int i = 0, j;
    if (!mask || (mask & (SPEAKER_ALL | SPEAKER_RESERVED)))
        mask = get_channel_mask(channels);
    for (j = 0; j < ARRAY_SIZE(pos_from_wfx) && i < channels; ++j)
        if (mask & (1u << j))
            info->position[i++] = pos_from_wfx[j];
    if (mask == SPEAKER_FRONT_CENTER && channels == 1)
        info->position[0] = SPA_AUDIO_CHANNEL_MONO;
    for (; i < channels; ++i)
        info->position[i] = SPA_AUDIO_CHANNEL_UNKNOWN;
}

/* ------------------------------------------------------------------ *
 *  Connection + core sync                                             *
 * ------------------------------------------------------------------ */

static void on_core_done(void *data, uint32_t id, int seq)
{
    if (id == PW_ID_CORE && seq == core_sync_seq) {
        core_sync_done = TRUE;
        pw_thread_loop_signal(pw_loop, FALSE);
    }
}

static void on_core_error(void *data, uint32_t id, int seq, int res, const char *message)
{
    WARN("core error id:%u seq:%d res:%d (%s): %s\n", id, seq, res, spa_strerror(res), message);
    if (id == PW_ID_CORE) {
        core_sync_done = TRUE;
        pw_thread_loop_signal(pw_loop, FALSE);
    }
}

static const struct pw_core_events core_events = {
    PW_VERSION_CORE_EVENTS,
    .done  = on_core_done,
    .error = on_core_error,
};

/* Caller must hold the loop lock. Round-trips the server so that all
 * pending registry globals have been delivered. */
static void core_roundtrip(void)
{
    if (!pw_core) return;
    core_sync_done = FALSE;
    core_sync_seq = pw_core_sync(pw_core, PW_ID_CORE, 0);
    while (!core_sync_done)
        pw_thread_loop_wait(pw_loop);
}

/* ------------------------------------------------------------------ *
 *  Registry enumeration -> device lists                               *
 * ------------------------------------------------------------------ */

static void pw_add_device(struct list *list, const struct spa_dict *props,
                          UINT index, EndpointFormFactor form, UINT channel_mask,
                          const char *node_name, const char *desc, UINT node_id)
{
    size_t len = strlen(node_name);
    PhysDevice *dev = malloc(FIELD_OFFSET(PhysDevice, pw_name[len + 1]));
    const char *buffer;
    int wlen;

    if (!dev)
        return;

    if (!desc || !desc[0])
        desc = node_name;
    wlen = ntdll_umbstowcs(desc, strlen(desc) + 1, NULL, 0);
    if (!(dev->name = malloc(wlen * sizeof(WCHAR)))) {
        free(dev);
        return;
    }
    ntdll_umbstowcs(desc, strlen(desc) + 1, dev->name, wlen);

    dev->form = form;
    dev->index = index;
    dev->node_id = node_id;
    dev->channel_mask = channel_mask;
    dev->def_period = 100000;   /* 10 ms default */
    dev->min_period = 30000;    /* 3 ms minimum  */
    dev->bus_type = phys_device_bus_invalid;
    dev->vendor_id = dev->product_id = 0;

    if (props) {
        if ((buffer = spa_dict_lookup(props, "device.bus"))) {
            if (!strcmp(buffer, "usb"))      dev->bus_type = phys_device_bus_usb;
            else if (!strcmp(buffer, "pci")) dev->bus_type = phys_device_bus_pci;
        }
        if ((buffer = spa_dict_lookup(props, "device.vendor.id")))
            dev->vendor_id = strtol(buffer, NULL, 16);
        if ((buffer = spa_dict_lookup(props, "device.product.id")))
            dev->product_id = strtol(buffer, NULL, 16);
    }

    memcpy(dev->pw_name, node_name, len + 1);
    list_add_tail(list, &dev->entry);
    TRACE("added %s (%s)\n", debugstr_w(dev->name), node_name);
}

static void on_registry_global(void *data, uint32_t id, uint32_t permissions,
                               const char *type, uint32_t version,
                               const struct spa_dict *props)
{
    const char *media_class, *node_name, *desc, *serial;
    UINT index;

    if (!props || !type)
        return;

    if (!strcmp(type, PW_TYPE_INTERFACE_Metadata)) {
        const char *meta_name = spa_dict_lookup(props, PW_KEY_METADATA_NAME);
        if (meta_name && !strcmp(meta_name, "default"))
            g_metadata_id = id;
        return;
    }

    if (strcmp(type, PW_TYPE_INTERFACE_Node))
        return;

    media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (!media_class)
        return;

    node_name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    if (!node_name)
        return;
    desc   = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
    serial = spa_dict_lookup(props, PW_KEY_OBJECT_SERIAL);
    index  = serial ? strtoul(serial, NULL, 10) : id;

    /* TUNING: we report a fixed stereo mask. To report the true channel
     * layout, enumerate the node's EnumFormat param (requires binding the
     * node proxy and a param roundtrip). Stereo is what Windows reports as
     * the shared-mode mix layout for the vast majority of endpoints. */
    if (!strcmp(media_class, "Audio/Sink"))
        pw_add_device(&g_phys_speakers, props, index, Speakers,
                      KSAUDIO_SPEAKER_STEREO, node_name, desc, id);
    else if (!strcmp(media_class, "Audio/Source"))
        pw_add_device(&g_phys_sources, props, index, Microphone,
                      0, node_name, desc, id);
}

static void on_registry_global_remove(void *data, uint32_t id)
{
    /* Endpoints are rebuilt on each test_connect; nothing to do here for
     * the static lists. A live hot-plug path would remove by id. */
}

static const struct pw_registry_events registry_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global        = on_registry_global,
    .global_remove = on_registry_global_remove,
};

/* ------------------------------------------------------------------ *
 *  Default per-device format (replaces pulse_probe_settings)          *
 * ------------------------------------------------------------------ */

static void fill_device_format(PhysDevice *dev, int render)
{
    WAVEFORMATEXTENSIBLE *fmt = &dev->fmt;
    WAVEFORMATEX *wfx = &fmt->Format;
    UINT channels = 2;
    UINT mask = render ? dev->channel_mask : 0;

    if (!mask) mask = KSAUDIO_SPEAKER_STEREO;

    /* PipeWire mixes/converts freely; advertise its native shared format:
     * 32-bit float, stereo, 48 kHz — which is exactly what Windows reports
     * as the shared-mode mix format for a typical endpoint. */
    wfx->wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
    wfx->cbSize          = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfx->nChannels       = channels;
    wfx->nSamplesPerSec  = 48000;
    wfx->wBitsPerSample  = 32;
    wfx->nBlockAlign     = wfx->nChannels * wfx->wBitsPerSample / 8;
    wfx->nAvgBytesPerSec = wfx->nSamplesPerSec * wfx->nBlockAlign;
    fmt->Samples.wValidBitsPerSample = 32;
    fmt->dwChannelMask   = mask;
    fmt->SubFormat       = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
}

/* ------------------------------------------------------------------ *
 *  Connect / disconnect                                               *
 * ------------------------------------------------------------------ */

static HRESULT ensure_loop(void)
{
    if (pw_loop)
        return S_OK;

    pw_loop = pw_thread_loop_new("yetios-audio", NULL);
    if (!pw_loop) {
        ERR("pw_thread_loop_new failed\n");
        return E_FAIL;
    }
    if (pw_thread_loop_start(pw_loop) < 0) {
        ERR("pw_thread_loop_start failed\n");
        pw_thread_loop_destroy(pw_loop);
        pw_loop = NULL;
        return E_FAIL;
    }
    return S_OK;
}

/* Caller must hold the loop lock. */
static HRESULT pw_connect(void)
{
    if (pw_core)
        return S_OK;

    pw_ctx = pw_context_new(pw_thread_loop_get_loop(pw_loop), NULL, 0);
    if (!pw_ctx) {
        ERR("pw_context_new failed\n");
        return E_FAIL;
    }
    pw_core = pw_context_connect(pw_ctx, NULL, 0);
    if (!pw_core) {
        ERR("pw_context_connect failed (is the PipeWire daemon running?)\n");
        pw_context_destroy(pw_ctx);
        pw_ctx = NULL;
        return AUDCLNT_E_SERVICE_NOT_RUNNING;
    }

    spa_zero(core_listener);
    pw_core_add_listener(pw_core, &core_listener, &core_events, NULL);

    pw_reg = pw_core_get_registry(pw_core, PW_VERSION_REGISTRY, 0);
    spa_zero(reg_listener);
    pw_registry_add_listener(pw_reg, &reg_listener, &registry_events, NULL);

    return S_OK;
}

/* Caller must hold the loop lock. */
static void pw_disconnect(void)
{
    if (pw_reg)  { spa_hook_remove(&reg_listener);  pw_proxy_destroy((struct pw_proxy *)pw_reg); pw_reg = NULL; }
    if (pw_core) { spa_hook_remove(&core_listener); pw_core_disconnect(pw_core); pw_core = NULL; }
    if (pw_ctx)  { pw_context_destroy(pw_ctx); pw_ctx = NULL; }
}

/* ------------------------------------------------------------------ *
 *  process_attach / detach / main_loop                                *
 * ------------------------------------------------------------------ */

static NTSTATUS pw_process_attach(void *args)
{
    pw_init(NULL, NULL);

#ifdef _WIN64
    if (NtCurrentTeb()->WowTebOffset) {
        SYSTEM_BASIC_INFORMATION info;
        NtQuerySystemInformation(SystemEmulationBasicInformation, &info, sizeof(info), NULL);
        zero_bits = (ULONG_PTR)info.HighestUserAddress | 0x7fffffff;
    }
#endif
    return STATUS_SUCCESS;
}

static NTSTATUS pw_process_detach(void *args)
{
    if (pw_loop) {
        pw_lock();
        pw_disconnect();
        pw_unlock();
    }

    pthread_mutex_lock(&loop_park_mutex);
    loop_park_quit = TRUE;
    pthread_cond_broadcast(&loop_park_cond);
    pthread_mutex_unlock(&loop_park_mutex);

    if (pw_loop) {
        pw_thread_loop_stop(pw_loop);
        pw_thread_loop_destroy(pw_loop);
        pw_loop = NULL;
    }

    free_phys_device_lists();
    pw_deinit();
    return STATUS_SUCCESS;
}

/* mmdevapi runs this on a dedicated TIME_CRITICAL thread and expects it to
 * block for the lifetime of the backend. pw_thread_loop already owns the
 * real processing thread, so we just signal readiness and park here until
 * process_detach. */
static NTSTATUS pw_main_loop(void *args)
{
    struct main_loop_params *params = args;

    if (FAILED(ensure_loop())) {
        NtSetEvent(params->event, NULL);
        return STATUS_SUCCESS;
    }

    NtSetEvent(params->event, NULL);

    pthread_mutex_lock(&loop_park_mutex);
    while (!loop_park_quit)
        pthread_cond_wait(&loop_park_cond, &loop_park_mutex);
    pthread_mutex_unlock(&loop_park_mutex);

    return STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ *
 *  test_connect / get_endpoint_ids                                    *
 * ------------------------------------------------------------------ */

static NTSTATUS pw_test_connect(void *args)
{
    struct test_connect_params *params = args;
    PhysDevice *dev;

    if (FAILED(ensure_loop())) {
        params->priority = Priority_Unavailable;
        return STATUS_SUCCESS;
    }

    pw_lock();

    if (FAILED(pw_connect())) {
        pw_unlock();
        params->priority = Priority_Unavailable;
        return STATUS_SUCCESS;
    }

    free_phys_device_lists();
    list_init(&g_phys_speakers);
    list_init(&g_phys_sources);

    /* Fallback "default" entries (empty node name => server default), so a
     * stream can always be created even before enumeration completes. */
    pw_add_device(&g_phys_speakers, NULL, 0, Speakers, KSAUDIO_SPEAKER_STEREO, "", "PipeWire Output", 0);
    pw_add_device(&g_phys_sources,  NULL, 0, Microphone, 0, "", "PipeWire Input", 0);

    core_roundtrip();   /* deliver all registry globals */

    LIST_FOR_EACH_ENTRY(dev, &g_phys_speakers, PhysDevice, entry)
        fill_device_format(dev, 1);
    LIST_FOR_EACH_ENTRY(dev, &g_phys_sources, PhysDevice, entry)
        fill_device_format(dev, 0);

    /* Keep the connection alive for stream creation. */
    pw_unlock();

    params->priority = Priority_Preferred;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_get_endpoint_ids(void *args)
{
    struct get_endpoint_ids_params *params = args;
    struct list *list = (params->flow == eRender) ? &g_phys_speakers : &g_phys_sources;
    struct endpoint *endpoint = params->endpoints;
    size_t len, name_len, needed;
    unsigned int offset;
    PhysDevice *dev;

    params->num = list_count(list);
    offset = needed = params->num * sizeof(*params->endpoints);

    LIST_FOR_EACH_ENTRY(dev, list, PhysDevice, entry) {
        name_len = lstrlenW(dev->name) + 1;
        len = strlen(dev->pw_name) + 1;
        needed += name_len * sizeof(WCHAR) + ((len + 1) & ~1);

        if (needed <= params->size) {
            endpoint->name = offset;
            memcpy((char *)params->endpoints + offset, dev->name, name_len * sizeof(WCHAR));
            offset += name_len * sizeof(WCHAR);
            endpoint->device = offset;
            memcpy((char *)params->endpoints + offset, dev->pw_name, len);
            offset += (len + 1) & ~1;
            endpoint++;
        }
    }
    params->default_idx = 0;

    if (needed > params->size) {
        params->size = needed;
        params->result = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    } else
        params->result = S_OK;
    return STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ *
 *  Format negotiation                                                 *
 * ------------------------------------------------------------------ */

static HRESULT spec_from_waveformat(struct pw_audio_stream *stream, const WAVEFORMATEX *fmt)
{
    stream->ss.rate     = fmt->nSamplesPerSec;
    stream->ss.channels = fmt->nChannels;
    stream->ss.format   = PW_FMT_INVALID;
    stream->channel_mask = 0;

    if (!fmt->nChannels || fmt->nChannels > PW_CHANNELS_MAX)
        return AUDCLNT_E_UNSUPPORTED_FORMAT;

    switch (fmt->wFormatTag) {
    case WAVE_FORMAT_IEEE_FLOAT:
        if (fmt->wBitsPerSample == 32) stream->ss.format = PW_FMT_F32;
        stream->channel_mask = get_channel_mask(fmt->nChannels);
        break;
    case WAVE_FORMAT_PCM:
        if (fmt->wBitsPerSample == 8)       stream->ss.format = PW_FMT_U8;
        else if (fmt->wBitsPerSample == 16) stream->ss.format = PW_FMT_S16;
        else if (fmt->wBitsPerSample == 32) stream->ss.format = PW_FMT_S32;
        else return AUDCLNT_E_UNSUPPORTED_FORMAT;
        stream->channel_mask = get_channel_mask(fmt->nChannels);
        break;
    case WAVE_FORMAT_EXTENSIBLE: {
        WAVEFORMATEXTENSIBLE *wfe = (WAVEFORMATEXTENSIBLE *)fmt;
        DWORD valid = wfe->Samples.wValidBitsPerSample;
        if (fmt->cbSize != (sizeof(*wfe) - sizeof(*fmt)) && fmt->cbSize != sizeof(*wfe))
            return AUDCLNT_E_UNSUPPORTED_FORMAT;
        if (!valid) valid = fmt->wBitsPerSample;
        stream->channel_mask = wfe->dwChannelMask;

        if (IsEqualGUID(&wfe->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) &&
            fmt->wBitsPerSample == 32 && (valid == 32 || !wfe->Samples.wValidBitsPerSample))
            stream->ss.format = PW_FMT_F32;
        else if (IsEqualGUID(&wfe->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM)) {
            switch (fmt->wBitsPerSample) {
            case 8:  if (valid == 8)  stream->ss.format = PW_FMT_U8;  break;
            case 16: if (valid == 16) stream->ss.format = PW_FMT_S16; break;
            case 24: if (valid == 24) stream->ss.format = PW_FMT_S24; break;
            case 32:
                if (valid == 24)      stream->ss.format = PW_FMT_S24_32;
                else if (valid == 32) stream->ss.format = PW_FMT_S32;
                break;
            default: return AUDCLNT_E_UNSUPPORTED_FORMAT;
            }
        }
        break;
    }
    default:
        WARN("Unhandled tag %x\n", fmt->wFormatTag);
        return AUDCLNT_E_UNSUPPORTED_FORMAT;
    }

    if (stream->ss.format == PW_FMT_INVALID)
        return AUDCLNT_E_UNSUPPORTED_FORMAT;
    return S_OK;
}

static const struct spa_pod *build_format_pod(struct pw_audio_stream *stream,
                                              struct spa_pod_builder *b)
{
    struct spa_audio_info_raw info;
    spa_zero(info);
    info.format   = spa_format_from_fmt(stream->ss.format);
    info.rate     = stream->ss.rate;
    info.channels = stream->ss.channels;
    fill_spa_position(&info, stream->channel_mask, stream->ss.channels);
    return spa_format_audio_raw_build(b, SPA_PARAM_EnumFormat, &info);
}

/* ------------------------------------------------------------------ *
 *  Software volume application (carried over from pulse write_buffer) *
 * ------------------------------------------------------------------ */

static void apply_volume(const struct pw_audio_stream *stream, BYTE *buffer, UINT32 bytes)
{
    const float *vol = stream->vol;
    UINT32 i, channels, mute = 0;
    BOOL adjust = FALSE;
    BYTE *end;

    if (!bytes) return;
    channels = stream->ss.channels;
    for (i = 0; i < channels; i++) {
        adjust |= vol[i] != 1.0f;
        if (vol[i] == 0.0f) mute++;
    }
    if (mute == channels) { silence_buffer(stream->ss.format, buffer, bytes); return; }
    if (!adjust) return;

    end = buffer + bytes;
    switch (stream->ss.format) {
#ifndef WORDS_BIGENDIAN
#define PROCESS_BUFFER(type) do {            \
    type *p = (type*)buffer;                 \
    do {                                     \
        for (i = 0; i < channels; i++)       \
            p[i] = p[i] * vol[i];            \
        p += i;                              \
    } while ((BYTE*)p != end);               \
} while (0)
    case PW_FMT_S16: PROCESS_BUFFER(INT16); break;
    case PW_FMT_S32: PROCESS_BUFFER(INT32); break;
    case PW_FMT_F32: PROCESS_BUFFER(float); break;
#undef PROCESS_BUFFER
    case PW_FMT_S24_32: {
        UINT32 *p = (UINT32*)buffer;
        do {
            for (i = 0; i < channels; i++) {
                p[i] = (INT32)((INT32)(p[i] << 8) * vol[i]);
                p[i] >>= 8;
            }
            p += i;
        } while ((BYTE*)p != end);
        break;
    }
    case PW_FMT_S24: {
        BYTE *p = buffer;
        i = 0;
        while (p != end) {
            INT32 s = (p[0] << 8) | (p[1] << 16) | (p[2] << 24);
            s = (INT32)(s * vol[i]);
            p[0] = (s >> 8) & 0xff; p[1] = (s >> 16) & 0xff; p[2] = (s >> 24) & 0xff;
            p += 3;
            if (++i == channels) i = 0;
        }
        break;
    }
#endif
    case PW_FMT_U8: {
        UINT8 *p = (UINT8*)buffer;
        do {
            for (i = 0; i < channels; i++)
                p[i] = (int)((p[i] - 128) * vol[i]) + 128;
            p += i;
        } while ((BYTE*)p != end);
        break;
    }
    default:
        break;
    }
}

/* ------------------------------------------------------------------ *
 *  pw_stream process callbacks                                        *
 *                                                                     *
 *  Render : pull from local ring -> SPA buffer (server asks us).      *
 *  Capture: push SPA buffer -> packet ring.                           *
 *                                                                     *
 *  These run on the pw_thread_loop thread; the loop lock is held by   *
 *  pipewire during the callback, which is the same lock pw_lock()     *
 *  takes, so the ring accounting is serialized against the unix-call  *
 *  side. (NtSetEvent/NtQueryPerformanceCounter are thread-safe.)      *
 * ------------------------------------------------------------------ */

static void on_render_process(void *userdata)
{
    struct pw_audio_stream *stream = userdata;
    struct pw_buffer *pwbuf;
    struct spa_buffer *buf;
    UINT32 fsize = frame_size(&stream->ss);
    UINT32 req, avail, to_copy, chunk;
    BYTE *dst, *src;

    if (!(pwbuf = pw_stream_dequeue_buffer(stream->stream)))
        return;
    buf = pwbuf->buffer;
    dst = buf->datas[0].data;
    if (!dst) { pw_stream_queue_buffer(stream->stream, pwbuf); return; }

    req = buf->datas[0].maxsize / fsize;
#ifdef SPA_VERSION_BUFFER
    if (pwbuf->requested && pwbuf->requested < req)
        req = pwbuf->requested;
#endif
    avail = stream->held_bytes / fsize;
    to_copy = min(req, avail);

    /* Copy out of the local ring, honouring wrap-around. */
    {
        UINT32 copied = 0;
        UINT32 bytes = to_copy * fsize;
        while (copied < bytes) {
            src = stream->local_buffer + stream->lcl_offs_bytes;
            chunk = stream->real_bufsize_bytes - stream->lcl_offs_bytes;
            if (chunk > bytes - copied) chunk = bytes - copied;
            memcpy(dst + copied, src, chunk);
            copied += chunk;
            stream->lcl_offs_bytes = (stream->lcl_offs_bytes + chunk) % stream->real_bufsize_bytes;
        }
        if (bytes) {
            apply_volume(stream, dst, bytes);
            stream->held_bytes -= bytes;
        }
        /* Pad the remainder of the requested quantum with silence on
         * underrun so the graph keeps a steady cadence. */
        if (to_copy < req) {
            silence_buffer(stream->ss.format, dst + bytes, (req - to_copy) * fsize);
            stream->just_underran = TRUE;
        }
    }

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = fsize;
    buf->datas[0].chunk->size   = req * fsize;

    pw_stream_queue_buffer(stream->stream, pwbuf);
}

static void on_capture_process(void *userdata)
{
    struct pw_audio_stream *stream = userdata;
    struct pw_buffer *pwbuf;
    struct spa_buffer *buf;
    BYTE *src;
    UINT32 src_bytes, rem, copy;

    if (!(pwbuf = pw_stream_dequeue_buffer(stream->stream)))
        return;
    buf = pwbuf->buffer;
    src = buf->datas[0].data;
    if (!src) { pw_stream_queue_buffer(stream->stream, pwbuf); return; }

    src_bytes = buf->datas[0].chunk->size;
    rem = src_bytes;

    while (rem >= stream->period_bytes || (stream->peek_len && rem)) {
        ACPacket *p;
        BYTE *dst;
        UINT32 want;
        LARGE_INTEGER stamp, freq;

        if (!(p = (ACPacket *)list_head(&stream->packet_free_head))) {
            /* ring full: drop oldest filled packet (discontinuity) */
            p = (ACPacket *)list_head(&stream->packet_filled_head);
            if (!p) break;
            list_remove(&p->entry);
            ((ACPacket *)list_head(&stream->packet_filled_head))->discont = 1;
        } else {
            list_remove(&p->entry);
            stream->held_bytes += stream->period_bytes;
        }

        NtQueryPerformanceCounter(&stamp, &freq);
        p->qpcpos = (stamp.QuadPart * (INT64)10000000) / freq.QuadPart;
        p->discont = 0;

        dst = p->data;
        want = stream->period_bytes;
        copy = min(want, rem);
        memcpy(dst, src + (src_bytes - rem), copy);
        if (copy < want)
            silence_buffer(stream->ss.format, dst + copy, want - copy);
        rem -= copy;

        list_add_tail(&stream->packet_filled_head, &p->entry);
    }

    pw_stream_queue_buffer(stream->stream, pwbuf);
}

static void on_stream_state_changed(void *userdata, enum pw_stream_state old,
                                    enum pw_stream_state state, const char *error)
{
    TRACE("stream %p: %s -> %s%s%s\n", userdata,
          pw_stream_state_as_string(old), pw_stream_state_as_string(state),
          error ? " err=" : "", error ? error : "");
    pw_thread_loop_signal(pw_loop, FALSE);
}

static const struct pw_stream_events render_stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .state_changed = on_stream_state_changed,
    .process       = on_render_process,
};

static const struct pw_stream_events capture_stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .state_changed = on_stream_state_changed,
    .process       = on_capture_process,
};

/* Caller holds the loop lock. */
static HRESULT stream_connect(struct pw_audio_stream *stream, const char *node_name)
{
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[1];
    struct pw_properties *props;
    enum pw_stream_state st;
    const char *role = (stream->dataflow == eRender) ? "Music" : "Production";
    int res;

    props = pw_properties_new(
        PW_KEY_MEDIA_TYPE,     "Audio",
        PW_KEY_MEDIA_CATEGORY, (stream->dataflow == eRender) ? "Playback" : "Capture",
        PW_KEY_MEDIA_ROLE,     role,
        PW_KEY_APP_NAME,       "YetiOS",
        NULL);
    if (!props)
        return E_OUTOFMEMORY;

    /* Pin to a specific endpoint if one was requested. */
    if (node_name && node_name[0])
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, node_name);

    /* Request a quantum close to the WASAPI period. */
    pw_properties_setf(props, PW_KEY_NODE_LATENCY, "%u/%u",
                       (unsigned)(stream->period_bytes / frame_size(&stream->ss)),
                       stream->ss.rate);

    stream->stream = pw_stream_new(pw_core, "YetiOS audio stream", props);
    if (!stream->stream)
        return AUDCLNT_E_ENDPOINT_CREATE_FAILED;

    spa_zero(stream->stream_listener);
    pw_stream_add_listener(stream->stream, &stream->stream_listener,
                           (stream->dataflow == eRender) ? &render_stream_events
                                                         : &capture_stream_events,
                           stream);

    params[0] = build_format_pod(stream, &b);

    res = pw_stream_connect(stream->stream,
                            (stream->dataflow == eRender) ? PW_DIRECTION_OUTPUT
                                                          : PW_DIRECTION_INPUT,
                            PW_ID_ANY,
                            PW_STREAM_FLAG_AUTOCONNECT |
                            PW_STREAM_FLAG_MAP_BUFFERS |
                            PW_STREAM_FLAG_RT_PROCESS,
                            params, 1);
    if (res < 0) {
        WARN("pw_stream_connect failed: %s\n", spa_strerror(res));
        return AUDCLNT_E_ENDPOINT_CREATE_FAILED;
    }

    /* Wait for the stream to become paused/streaming or error out. */
    while ((st = pw_stream_get_state(stream->stream, NULL)) != PW_STREAM_STATE_PAUSED &&
            st != PW_STREAM_STATE_STREAMING && st != PW_STREAM_STATE_ERROR)
        pw_thread_loop_wait(pw_loop);

    if (st == PW_STREAM_STATE_ERROR)
        return AUDCLNT_E_ENDPOINT_CREATE_FAILED;

    return S_OK;
}

static BOOL stream_valid(struct pw_audio_stream *stream)
{
    enum pw_stream_state st;
    if (!stream || !stream->stream) return FALSE;
    st = pw_stream_get_state(stream->stream, NULL);
    return st == PW_STREAM_STATE_PAUSED || st == PW_STREAM_STATE_STREAMING;
}

/* ------------------------------------------------------------------ *
 *  create_stream / release_stream                                     *
 * ------------------------------------------------------------------ */

static NTSTATUS pw_create_stream(void *args)
{
    struct create_stream_params *params = args;
    struct pw_audio_stream *stream;
    unsigned int i, bufsize_bytes;
    HRESULT hr;

    if (params->share == AUDCLNT_SHAREMODE_EXCLUSIVE) {
        /* PipeWire has no true exclusive mode; treat as shared. Mark this
         * for the Advanced-tab exclusive checkboxes later. */
        params->result = AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED;
        return STATUS_SUCCESS;
    }

    pw_lock();

    if (FAILED(params->result = pw_connect())) {
        pw_unlock();
        return STATUS_SUCCESS;
    }

    if (!(stream = calloc(1, sizeof(*stream)))) {
        pw_unlock();
        params->result = E_OUTOFMEMORY;
        return STATUS_SUCCESS;
    }

    stream->dataflow = params->flow;
    for (i = 0; i < ARRAY_SIZE(stream->vol); ++i)
        stream->vol[i] = 1.0f;

    hr = spec_from_waveformat(stream, params->fmt);
    if (FAILED(hr))
        goto exit;

    stream->def_period = params->period;
    stream->duration   = params->duration;
    stream->period_bytes = frame_size(&stream->ss) *
        muldiv(params->period, stream->ss.rate, 10000000);
    stream->bufsize_frames = ceil((params->duration / 10000000.) * params->fmt->nSamplesPerSec);
    bufsize_bytes = stream->bufsize_frames * frame_size(&stream->ss);
    stream->mmdev_period_usec = params->period / 10;
    stream->share = params->share;
    stream->flags = params->flags;

    hr = stream_connect(stream, params->device);
    if (SUCCEEDED(hr)) {
        SIZE_T size;
        if (stream->dataflow == eRender) {
            size = stream->real_bufsize_bytes =
                stream->bufsize_frames * 2 * frame_size(&stream->ss);
            if (NtAllocateVirtualMemory(GetCurrentProcess(), (void **)&stream->local_buffer,
                                        zero_bits, &size, MEM_COMMIT, PAGE_READWRITE))
                hr = E_OUTOFMEMORY;
        } else {
            UINT32 capture_packets, unalign;
            if ((unalign = bufsize_bytes % stream->period_bytes))
                bufsize_bytes += stream->period_bytes - unalign;
            stream->bufsize_frames = bufsize_bytes / frame_size(&stream->ss);
            stream->real_bufsize_bytes = bufsize_bytes;
            capture_packets = stream->real_bufsize_bytes / stream->period_bytes;
            size = stream->real_bufsize_bytes + capture_packets * sizeof(ACPacket);
            if (NtAllocateVirtualMemory(GetCurrentProcess(), (void **)&stream->local_buffer,
                                        zero_bits, &size, MEM_COMMIT, PAGE_READWRITE))
                hr = E_OUTOFMEMORY;
            else {
                ACPacket *cur = (ACPacket *)(stream->local_buffer + stream->real_bufsize_bytes);
                BYTE *data = stream->local_buffer;
                silence_buffer(stream->ss.format, stream->local_buffer, stream->real_bufsize_bytes);
                list_init(&stream->packet_free_head);
                list_init(&stream->packet_filled_head);
                for (i = 0; i < capture_packets; ++i, ++cur) {
                    list_add_tail(&stream->packet_free_head, &cur->entry);
                    cur->data = data;
                    data += stream->period_bytes;
                }
            }
        }
    }

    *params->channel_count = stream->ss.channels;
    *params->stream = (stream_handle)(UINT_PTR)stream;

exit:
    if (FAILED(params->result = hr)) {
        if (stream->local_buffer) {
            SIZE_T size = 0;
            NtFreeVirtualMemory(GetCurrentProcess(), (void **)&stream->local_buffer, &size, MEM_RELEASE);
        }
        if (stream->stream) {
            spa_hook_remove(&stream->stream_listener);
            pw_stream_destroy(stream->stream);
        }
        free(stream);
    }

    pw_unlock();
    return STATUS_SUCCESS;
}

static NTSTATUS pw_release_stream(void *args)
{
    struct release_stream_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);
    SIZE_T size;

    if (params->timer_thread) {
        stream->please_quit = TRUE;
        NtWaitForSingleObject(params->timer_thread, FALSE, NULL);
        NtClose(params->timer_thread);
    }

    pw_lock();
    if (stream->stream) {
        spa_hook_remove(&stream->stream_listener);
        pw_stream_destroy(stream->stream);
        stream->stream = NULL;
    }
    pw_unlock();

    if (stream->local_buffer) {
        size = 0;
        NtFreeVirtualMemory(GetCurrentProcess(), (void **)&stream->local_buffer, &size, MEM_RELEASE);
    }
    if (stream->tmp_buffer) {
        size = 0;
        NtFreeVirtualMemory(GetCurrentProcess(), (void **)&stream->tmp_buffer, &size, MEM_RELEASE);
    }
    free(stream->peek_buffer);
    free(stream);

    params->result = S_OK;
    return STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ *
 *  start / stop / reset                                               *
 * ------------------------------------------------------------------ */

static NTSTATUS pw_start(void *args)
{
    struct start_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);

    params->result = S_OK;
    pw_lock();
    if (!stream_valid(stream)) { pw_unlock(); params->result = S_OK; return STATUS_SUCCESS; }

    if ((stream->flags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK) && !stream->event) {
        pw_unlock();
        params->result = AUDCLNT_E_EVENTHANDLE_NOT_SET;
        return STATUS_SUCCESS;
    }
    if (stream->started) {
        pw_unlock();
        params->result = AUDCLNT_E_NOT_STOPPED;
        return STATUS_SUCCESS;
    }

    pw_stream_set_active(stream->stream, TRUE);
    stream->started = TRUE;
    stream->just_started = TRUE;
    pw_unlock();
    return STATUS_SUCCESS;
}

static NTSTATUS pw_stop(void *args)
{
    struct stop_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);

    pw_lock();
    if (!stream_valid(stream)) { pw_unlock(); params->result = AUDCLNT_E_DEVICE_INVALIDATED; return STATUS_SUCCESS; }
    if (!stream->started)      { pw_unlock(); params->result = S_FALSE; return STATUS_SUCCESS; }

    pw_stream_set_active(stream->stream, FALSE);
    stream->started = FALSE;
    pw_unlock();
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_reset(void *args)
{
    struct reset_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);

    pw_lock();
    if (!stream_valid(stream)) { pw_unlock(); params->result = AUDCLNT_E_DEVICE_INVALIDATED; return STATUS_SUCCESS; }
    if (stream->started)       { pw_unlock(); params->result = AUDCLNT_E_NOT_STOPPED; return STATUS_SUCCESS; }
    if (stream->locked)        { pw_unlock(); params->result = AUDCLNT_E_BUFFER_OPERATION_PENDING; return STATUS_SUCCESS; }

    if (stream->dataflow == eRender) {
        stream->clock_lastpos = stream->clock_written = 0;
        stream->pw_offs_bytes = stream->lcl_offs_bytes = 0;
        stream->held_bytes = stream->pw_held_bytes = 0;
    } else {
        ACPacket *p;
        stream->clock_written += stream->held_bytes;
        stream->held_bytes = 0;
        if ((p = stream->locked_ptr)) {
            stream->locked_ptr = NULL;
            list_add_tail(&stream->packet_free_head, &p->entry);
        }
        list_move_tail(&stream->packet_free_head, &stream->packet_filled_head);
    }
    pw_unlock();
    params->result = S_OK;
    return STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ *
 *  timer_loop — paces the WASAPI event callback                       *
 * ------------------------------------------------------------------ */

static NTSTATUS pw_timer_loop(void *args)
{
    struct timer_loop_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);
    LARGE_INTEGER delay;

    delay.QuadPart = -(LONGLONG)stream->mmdev_period_usec * 10;

    while (!stream->please_quit) {
        NtDelayExecution(FALSE, &delay);
        if (stream->event)
            NtSetEvent(stream->event, NULL);
    }
    return STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ *
 *  Render buffer (producer side; on_render_process is consumer)       *
 * ------------------------------------------------------------------ */

static BOOL alloc_tmp_buffer(struct pw_audio_stream *stream, SIZE_T bytes)
{
    SIZE_T size;
    if (stream->tmp_buffer_bytes >= bytes) return TRUE;
    if (stream->tmp_buffer) {
        size = 0;
        NtFreeVirtualMemory(GetCurrentProcess(), (void **)&stream->tmp_buffer, &size, MEM_RELEASE);
        stream->tmp_buffer = NULL;
        stream->tmp_buffer_bytes = 0;
    }
    if (NtAllocateVirtualMemory(GetCurrentProcess(), (void **)&stream->tmp_buffer,
                                zero_bits, &bytes, MEM_COMMIT, PAGE_READWRITE))
        return FALSE;
    stream->tmp_buffer_bytes = bytes;
    return TRUE;
}

static UINT32 render_padding(struct pw_audio_stream *stream)
{
    return stream->held_bytes / frame_size(&stream->ss);
}

static UINT32 capture_padding(struct pw_audio_stream *stream)
{
    ACPacket *packet = stream->locked_ptr;
    if (!packet && !list_empty(&stream->packet_filled_head)) {
        packet = (ACPacket *)list_head(&stream->packet_filled_head);
        stream->locked_ptr = packet;
        list_remove(&packet->entry);
    }
    return stream->held_bytes / frame_size(&stream->ss);
}

static NTSTATUS pw_get_render_buffer(void *args)
{
    struct get_render_buffer_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);
    UINT32 fsize = frame_size(&stream->ss);
    size_t bytes;
    UINT32 wri_offs_bytes;

    pw_lock();
    if (!stream_valid(stream)) { pw_unlock(); params->result = AUDCLNT_E_DEVICE_INVALIDATED; return STATUS_SUCCESS; }
    if (stream->locked)        { pw_unlock(); params->result = AUDCLNT_E_OUT_OF_ORDER; return STATUS_SUCCESS; }
    if (!params->frames)       { pw_unlock(); *params->data = NULL; params->result = S_OK; return STATUS_SUCCESS; }

    if (stream->held_bytes / fsize + params->frames > stream->bufsize_frames) {
        pw_unlock();
        params->result = AUDCLNT_E_BUFFER_TOO_LARGE;
        return STATUS_SUCCESS;
    }

    bytes = params->frames * fsize;
    wri_offs_bytes = (stream->lcl_offs_bytes + stream->held_bytes) % stream->real_bufsize_bytes;
    if (wri_offs_bytes + bytes > stream->real_bufsize_bytes) {
        if (!alloc_tmp_buffer(stream, bytes)) { pw_unlock(); params->result = E_OUTOFMEMORY; return STATUS_SUCCESS; }
        *params->data = stream->tmp_buffer;
        stream->locked = -bytes;
    } else {
        *params->data = stream->local_buffer + wri_offs_bytes;
        stream->locked = bytes;
    }

    silence_buffer(stream->ss.format, *params->data, bytes);
    pw_unlock();
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static void wrap_buffer(struct pw_audio_stream *stream, BYTE *buffer, UINT32 written_bytes)
{
    UINT32 wri_offs_bytes = (stream->lcl_offs_bytes + stream->held_bytes) % stream->real_bufsize_bytes;
    UINT32 chunk_bytes = stream->real_bufsize_bytes - wri_offs_bytes;
    if (written_bytes <= chunk_bytes)
        memcpy(stream->local_buffer + wri_offs_bytes, buffer, written_bytes);
    else {
        memcpy(stream->local_buffer + wri_offs_bytes, buffer, chunk_bytes);
        memcpy(stream->local_buffer, buffer + chunk_bytes, written_bytes - chunk_bytes);
    }
}

static NTSTATUS pw_release_render_buffer(void *args)
{
    struct release_render_buffer_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);
    UINT32 fsize = frame_size(&stream->ss);
    UINT32 written_bytes;
    BYTE *buffer;

    pw_lock();
    if (!stream->locked || !params->written_frames) {
        stream->locked = 0;
        pw_unlock();
        params->result = params->written_frames ? AUDCLNT_E_OUT_OF_ORDER : S_OK;
        return STATUS_SUCCESS;
    }

    if (params->written_frames * fsize > (stream->locked >= 0 ? stream->locked : -stream->locked)) {
        pw_unlock();
        params->result = AUDCLNT_E_INVALID_SIZE;
        return STATUS_SUCCESS;
    }

    if (stream->locked >= 0)
        buffer = stream->local_buffer + (stream->lcl_offs_bytes + stream->held_bytes) % stream->real_bufsize_bytes;
    else
        buffer = stream->tmp_buffer;

    written_bytes = params->written_frames * fsize;
    if (params->flags & AUDCLNT_BUFFERFLAGS_SILENT)
        silence_buffer(stream->ss.format, buffer, written_bytes);
    if (stream->locked < 0)
        wrap_buffer(stream, buffer, written_bytes);

    stream->held_bytes += written_bytes;
    stream->clock_written += written_bytes;
    stream->locked = 0;

    pw_unlock();
    params->result = S_OK;
    return STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ *
 *  Capture buffer (consumer side; on_capture_process is producer)     *
 * ------------------------------------------------------------------ */

static NTSTATUS pw_get_capture_buffer(void *args)
{
    struct get_capture_buffer_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);
    UINT32 fsize = frame_size(&stream->ss);
    ACPacket *packet;

    pw_lock();
    if (!stream_valid(stream)) { pw_unlock(); params->result = AUDCLNT_E_DEVICE_INVALIDATED; return STATUS_SUCCESS; }
    if (stream->locked)        { pw_unlock(); params->result = AUDCLNT_E_OUT_OF_ORDER; return STATUS_SUCCESS; }

    capture_padding(stream);
    if ((packet = stream->locked_ptr)) {
        *params->frames = stream->period_bytes / fsize;
        *params->flags = 0;
        if (packet->discont) *params->flags |= AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY;
        if (params->devpos)
            *params->devpos = (packet->discont ? stream->clock_written + stream->period_bytes
                                               : stream->clock_written) / fsize;
        if (params->qpcpos) *params->qpcpos = packet->qpcpos;
        *params->data = packet->data;
    } else
        *params->frames = 0;

    stream->locked = *params->frames;
    pw_unlock();
    params->result = *params->frames ? S_OK : AUDCLNT_S_BUFFER_EMPTY;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_release_capture_buffer(void *args)
{
    struct release_capture_buffer_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);

    pw_lock();
    if (!stream->locked && params->done) { pw_unlock(); params->result = AUDCLNT_E_OUT_OF_ORDER; return STATUS_SUCCESS; }
    if (params->done && stream->locked != params->done) { pw_unlock(); params->result = AUDCLNT_E_INVALID_SIZE; return STATUS_SUCCESS; }

    if (params->done) {
        ACPacket *packet = stream->locked_ptr;
        stream->locked_ptr = NULL;
        stream->held_bytes -= stream->period_bytes;
        stream->clock_written += packet->discont ? 2 * stream->period_bytes : stream->period_bytes;
        list_add_tail(&stream->packet_free_head, &packet->entry);
    }
    stream->locked = 0;
    pw_unlock();
    params->result = S_OK;
    return STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ *
 *  Format / period / size queries                                     *
 * ------------------------------------------------------------------ */

static NTSTATUS pw_is_format_supported(void *args)
{
    struct is_format_supported_params *params = args;
    struct pw_audio_stream probe = {0};

    if (!params->fmt_in) { params->result = E_POINTER; return STATUS_SUCCESS; }
    if (params->share == AUDCLNT_SHAREMODE_EXCLUSIVE) {
        params->result = AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED;
        return STATUS_SUCCESS;
    }
    /* PipeWire converts freely in shared mode; accept any format we can map. */
    params->result = spec_from_waveformat(&probe, params->fmt_in);
    return STATUS_SUCCESS;
}

static NTSTATUS pw_get_loopback_capture_device(void *args)
{
    struct get_loopback_capture_device_params *params = args;
    params->result = E_NOTIMPL;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_get_mix_format(void *args)
{
    struct get_mix_format_params *params = args;
    struct list *list = (params->flow == eRender) ? &g_phys_speakers : &g_phys_sources;
    PhysDevice *dev;

    LIST_FOR_EACH_ENTRY(dev, list, PhysDevice, entry) {
        if (strcmp(params->device, dev->pw_name)) continue;
        *params->fmt = dev->fmt;
        params->result = S_OK;
        return STATUS_SUCCESS;
    }
    params->result = E_FAIL;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_get_device_period(void *args)
{
    struct get_device_period_params *params = args;
    struct list *list = (params->flow == eRender) ? &g_phys_speakers : &g_phys_sources;
    PhysDevice *dev;

    if (!params->def_period && !params->min_period) { params->result = E_POINTER; return STATUS_SUCCESS; }

    LIST_FOR_EACH_ENTRY(dev, list, PhysDevice, entry) {
        if (strcmp(params->device, dev->pw_name)) continue;
        if (params->def_period) *params->def_period = dev->def_period;
        if (params->min_period) *params->min_period = dev->min_period;
        params->result = S_OK;
        return STATUS_SUCCESS;
    }
    params->result = E_FAIL;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_get_buffer_size(void *args)
{
    struct get_buffer_size_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);

    pw_lock();
    if (!stream_valid(stream)) params->result = AUDCLNT_E_DEVICE_INVALIDATED;
    else { *params->frames = stream->bufsize_frames; params->result = S_OK; }
    pw_unlock();
    return STATUS_SUCCESS;
}

static NTSTATUS pw_get_latency(void *args)
{
    struct get_latency_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);

    pw_lock();
    if (!stream_valid(stream)) { pw_unlock(); params->result = AUDCLNT_E_DEVICE_INVALIDATED; return STATUS_SUCCESS; }
    /* One period plus the configured device period; refine from
     * pw_time.delay once timing reporting is wired. */
    *params->latency = stream->def_period;
    pw_unlock();
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_get_current_padding(void *args)
{
    struct get_current_padding_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);

    pw_lock();
    if (!stream_valid(stream)) { pw_unlock(); params->result = AUDCLNT_E_DEVICE_INVALIDATED; return STATUS_SUCCESS; }
    *params->padding = (stream->dataflow == eRender) ? render_padding(stream)
                                                     : capture_padding(stream);
    pw_unlock();
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_get_next_packet_size(void *args)
{
    struct get_next_packet_size_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);

    pw_lock();
    capture_padding(stream);
    *params->frames = stream->locked_ptr ? stream->period_bytes / frame_size(&stream->ss) : 0;
    pw_unlock();
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_get_frequency(void *args)
{
    struct get_frequency_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);

    pw_lock();
    if (!stream_valid(stream)) { pw_unlock(); params->result = AUDCLNT_E_DEVICE_INVALIDATED; return STATUS_SUCCESS; }
    *params->freq = stream->ss.rate;
    if (stream->share == AUDCLNT_SHAREMODE_SHARED)
        *params->freq *= frame_size(&stream->ss);
    pw_unlock();
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_get_position(void *args)
{
    struct get_position_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);

    pw_lock();
    if (!stream_valid(stream)) { pw_unlock(); params->result = AUDCLNT_E_DEVICE_INVALIDATED; return STATUS_SUCCESS; }

    *params->pos = stream->clock_written - stream->held_bytes;
    if (stream->share == AUDCLNT_SHAREMODE_EXCLUSIVE || params->device)
        *params->pos /= frame_size(&stream->ss);
    if (*params->pos < stream->clock_lastpos) *params->pos = stream->clock_lastpos;
    else stream->clock_lastpos = *params->pos;
    pw_unlock();

    if (params->qpctime) {
        LARGE_INTEGER stamp, freq;
        NtQueryPerformanceCounter(&stamp, &freq);
        *params->qpctime = (stamp.QuadPart * (INT64)10000000) / freq.QuadPart;
    }
    params->result = S_OK;
    return STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ *
 *  Volume / event handle / sample rate                                *
 * ------------------------------------------------------------------ */

static NTSTATUS pw_set_volumes(void *args)
{
    struct set_volumes_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);
    unsigned int i;

    for (i = 0; i < stream->ss.channels; i++)
        stream->vol[i] = params->volumes[i] * params->master_volume * params->session_volumes[i];
    return STATUS_SUCCESS;
}

static NTSTATUS pw_set_event_handle(void *args)
{
    struct set_event_handle_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);
    HRESULT hr = S_OK;

    pw_lock();
    if (!stream_valid(stream))                                   hr = AUDCLNT_E_DEVICE_INVALIDATED;
    else if (!(stream->flags & AUDCLNT_STREAMFLAGS_EVENTCALLBACK)) hr = AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED;
    else if (stream->event)                                      hr = HRESULT_FROM_WIN32(ERROR_INVALID_NAME);
    else stream->event = params->event;
    pw_unlock();
    params->result = hr;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_set_sample_rate(void *args)
{
    struct set_sample_rate_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);
    HRESULT hr = S_OK;

    pw_lock();
    if (!stream_valid(stream)) hr = AUDCLNT_E_DEVICE_INVALIDATED;
    else if (stream->dataflow != eRender) hr = E_NOTIMPL;
    else {
        /* Renegotiate the quantum/rate via a param update. For now reset the
         * ring and update bookkeeping; a full reconnect with the new rate is
         * the robust path if apps actually exercise this. */
        stream->clock_lastpos = stream->clock_written = 0;
        stream->pw_offs_bytes = stream->lcl_offs_bytes = 0;
        stream->held_bytes = stream->pw_held_bytes = 0;
        stream->ss.rate = params->rate;
        stream->period_bytes = frame_size(&stream->ss) *
            muldiv(stream->mmdev_period_usec, stream->ss.rate, 1000000);
        silence_buffer(stream->ss.format, stream->local_buffer, stream->real_bufsize_bytes);
    }
    pw_unlock();
    params->result = hr;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_is_started(void *args)
{
    struct is_started_params *params = args;
    struct pw_audio_stream *stream = handle_get_stream(params->stream);

    pw_lock();
    params->result = (stream_valid(stream) && stream->started) ? S_OK : S_FALSE;
    pw_unlock();
    return STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ *
 *  get_prop_value                                                     *
 * ------------------------------------------------------------------ */

static BOOL get_device_path(PhysDevice *dev, struct get_prop_value_params *params)
{
    const GUID *guid = params->guid;
    PROPVARIANT *out = params->value;
    UINT serial_number;
    char path[128];
    int len;

    serial_number = (guid->Data4[4] << 24) | (guid->Data4[5] << 16) |
                    (guid->Data4[6] << 8) | guid->Data4[7];

    switch (dev->bus_type) {
    case phys_device_bus_pci:
        len = sprintf(path, "{1}.HDAUDIO\\FUNC_01&VEN_%04X&DEV_%04X\\%u&%08X",
                      dev->vendor_id, dev->product_id, dev->index, serial_number);
        break;
    case phys_device_bus_usb:
        len = sprintf(path, "{1}.USB\\VID_%04X&PID_%04X\\%u&%08X",
                      dev->vendor_id, dev->product_id, dev->index, serial_number);
        break;
    default:
        len = sprintf(path, "{1}.ROOT\\MEDIA\\%04u", dev->index);
        break;
    }

    if (*params->buffer_size < ++len * sizeof(WCHAR)) {
        params->result = E_NOT_SUFFICIENT_BUFFER;
        *params->buffer_size = len * sizeof(WCHAR);
        return FALSE;
    }
    out->vt = VT_LPWSTR;
    out->pwszVal = params->buffer;
    ntdll_umbstowcs(path, len, out->pwszVal, len);
    params->result = S_OK;
    return TRUE;
}

static NTSTATUS pw_get_prop_value(void *args)
{
    static const GUID PKEY_AudioEndpoint_GUID = {
        0x1da5d803, 0xd492, 0x4edd, {0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e}
    };
    static const PROPERTYKEY devicepath_key = {
        {0xb3f8fa53, 0x0004, 0x438e, {0x90, 0x03, 0x51, 0xa4, 0x6e, 0x13, 0x9b, 0xfc}}, 2
    };
    struct get_prop_value_params *params = args;
    struct list *list = (params->flow == eRender) ? &g_phys_speakers : &g_phys_sources;
    PhysDevice *dev;

    LIST_FOR_EACH_ENTRY(dev, list, PhysDevice, entry) {
        if (strcmp(params->device, dev->pw_name)) continue;
        if (IsEqualPropertyKey(*params->prop, devicepath_key)) {
            get_device_path(dev, params);
            return STATUS_SUCCESS;
        } else if (IsEqualGUID(&params->prop->fmtid, &PKEY_AudioEndpoint_GUID)) {
            switch (params->prop->pid) {
            case 0:   /* FormFactor */
                params->value->vt = VT_UI4;
                params->value->ulVal = dev->form;
                params->result = S_OK;
                return STATUS_SUCCESS;
            case 3:   /* PhysicalSpeakers */
                if (!dev->channel_mask) break;
                params->value->vt = VT_UI4;
                params->value->ulVal = dev->channel_mask;
                params->result = S_OK;
                return STATUS_SUCCESS;
            }
        }
        params->result = E_NOTIMPL;
        return STATUS_SUCCESS;
    }
    params->result = E_FAIL;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_midi_get_driver(void *args)
{
    /* No PipeWire MIDI bridge yet; fall back to the ALSA seq MIDI driver. */
    static const WCHAR driver[] = {'a','l','s','a',0};
    memcpy(args, driver, sizeof(driver));
    return STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ *
 *  Phase 2: endpoint (node) master volume + mute                      *
 *                                                                     *
 *  These bind the PipeWire node proxy and read/write its Props param  *
 *  (SPA_PROP_channelVolumes / SPA_PROP_mute). This is the device-wide *
 *  master that IAudioEndpointVolume drives — separate from the        *
 *  per-stream software gain applied in set_volumes/apply_volume.      *
 *                                                                     *
 *  TUNING: channelVolumes is treated as the WASAPI "scalar" directly  *
 *  (linear amplitude 0..1). PipeWire's UI tools apply a cubic curve   *
 *  for perceptual feel; if the slider feels non-linear vs. Windows,   *
 *  cube on write / cube-root on read here.                            *
 * ------------------------------------------------------------------ */

struct node_props_result
{
    BOOL got;
    float volume;       /* averaged channelVolumes */
    int mute;           /* -1 = unknown */
    uint32_t channels;
};

static void on_node_param(void *data, int seq, uint32_t id, uint32_t index,
                          uint32_t next, const struct spa_pod *param)
{
    struct node_props_result *r = data;
    struct spa_pod_object *obj;
    struct spa_pod_prop *prop;

    if (id != SPA_PARAM_Props || !param ||
        !spa_pod_is_object_type(param, SPA_TYPE_OBJECT_Props))
        return;

    obj = (struct spa_pod_object *)param;
    SPA_POD_OBJECT_FOREACH(obj, prop) {
        switch (prop->key) {
        case SPA_PROP_channelVolumes: {
            float vols[PW_CHANNELS_MAX];
            uint32_t n = spa_pod_copy_array(&prop->value, SPA_TYPE_Float,
                                            vols, PW_CHANNELS_MAX);
            if (n) {
                float sum = 0.0f;
                uint32_t i;
                for (i = 0; i < n; i++) sum += vols[i];
                r->volume = sum / n;
                r->channels = n;
            }
            break;
        }
        case SPA_PROP_volume: {
            float v;
            if (spa_pod_get_float(&prop->value, &v) == 0 && !r->channels)
                r->volume = v;
            break;
        }
        case SPA_PROP_mute: {
            bool m;
            if (spa_pod_get_bool(&prop->value, &m) == 0)
                r->mute = m ? 1 : 0;
            break;
        }
        default: break;
        }
    }
    r->got = TRUE;
}

static const struct pw_node_events node_events =
{
    PW_VERSION_NODE_EVENTS,
    .param = on_node_param,
};

/* Caller holds the loop lock. */
static BOOL node_get_props(UINT node_id, float *volume, int *mute, uint32_t *channels)
{
    struct pw_node *node;
    struct spa_hook listener;
    struct node_props_result res = { .mute = -1 };
    uint32_t ids[1] = { SPA_PARAM_Props };

    if (!node_id || !pw_reg) return FALSE;

    node = pw_registry_bind(pw_reg, node_id, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0);
    if (!node) return FALSE;

    spa_zero(listener);
    pw_node_add_listener(node, &listener, &node_events, &res);
    pw_node_subscribe_params(node, ids, 1);
    core_roundtrip();   /* pump until the Props param has been delivered */
    spa_hook_remove(&listener);
    pw_proxy_destroy((struct pw_proxy *)node);

    if (!res.got) return FALSE;
    if (volume)   *volume = res.volume;
    if (mute)     *mute = res.mute;
    if (channels) *channels = res.channels ? res.channels : 2;
    return TRUE;
}

/* Caller holds the loop lock. vols==NULL leaves the volume unchanged;
 * mute<0 leaves mute unchanged. */
static BOOL node_set_props(UINT node_id, const float *vols, uint32_t channels, int mute)
{
    struct pw_node *node;
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    struct spa_pod_frame f;
    const struct spa_pod *pod;

    if (!node_id || !pw_reg) return FALSE;

    node = pw_registry_bind(pw_reg, node_id, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0);
    if (!node) return FALSE;

    spa_pod_builder_push_object(&b, &f, SPA_TYPE_OBJECT_Props, SPA_PARAM_Props);
    if (vols && channels) {
        spa_pod_builder_prop(&b, SPA_PROP_channelVolumes, 0);
        spa_pod_builder_array(&b, sizeof(float), SPA_TYPE_Float, channels, vols);
    }
    if (mute >= 0) {
        spa_pod_builder_prop(&b, SPA_PROP_mute, 0);
        spa_pod_builder_bool(&b, mute ? true : false);
    }
    pod = spa_pod_builder_pop(&b, &f);

    pw_node_set_param(node, SPA_PARAM_Props, 0, pod);
    core_roundtrip();
    pw_proxy_destroy((struct pw_proxy *)node);
    return TRUE;
}

/* Resolve a device name to a bindable node id. An empty name (the synthetic
 * default endpoint) resolves to the first real node of the right flow. */
static UINT resolve_node_id(EDataFlow flow, const char *name)
{
    struct list *list = (flow == eRender) ? &g_phys_speakers : &g_phys_sources;
    PhysDevice *dev;

    if (name && name[0]) {
        LIST_FOR_EACH_ENTRY(dev, list, PhysDevice, entry)
            if (dev->node_id && !strcmp(name, dev->pw_name))
                return dev->node_id;
    }
    LIST_FOR_EACH_ENTRY(dev, list, PhysDevice, entry)
        if (dev->node_id)
            return dev->node_id;
    return 0;
}

static NTSTATUS pw_set_endpoint_volume(void *args)
{
    struct set_endpoint_volume_params *params = args;
    UINT node_id;

    pw_lock();
    node_id = resolve_node_id(params->flow, params->device);
    if (!node_id) {
        pw_unlock();
        params->result = E_FAIL;
        return STATUS_SUCCESS;
    }

    if (params->level >= 0.0f) {
        float vols[PW_CHANNELS_MAX];
        uint32_t i, channels = 2;
        node_get_props(node_id, NULL, NULL, &channels);
        if (!channels || channels > PW_CHANNELS_MAX) channels = 2;
        for (i = 0; i < channels; i++) vols[i] = params->level;
        node_set_props(node_id, vols, channels, params->mute);
    } else {
        node_set_props(node_id, NULL, 0, params->mute);
    }
    pw_unlock();
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_get_endpoint_volume(void *args)
{
    struct get_endpoint_volume_params *params = args;
    UINT node_id;
    float vol = 1.0f;
    int mute = 0;

    pw_lock();
    node_id = resolve_node_id(params->flow, params->device);
    if (!node_id) {
        pw_unlock();
        params->result = E_FAIL;
        return STATUS_SUCCESS;
    }
    if (!node_get_props(node_id, &vol, &mute, NULL)) {
        pw_unlock();
        params->result = E_FAIL;
        return STATUS_SUCCESS;
    }
    pw_unlock();

    if (params->level) *params->level = vol;
    if (params->mute)  *params->mute = (mute < 0) ? 0 : mute;
    params->result = S_OK;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_get_endpoint_volume_range(void *args)
{
    struct get_endpoint_volume_range_params *params = args;
    /* PipeWire node volume is linear 0..1 with no fixed step; report the
     * conventional Windows shared-endpoint dB range. */
    if (params->min_db) *params->min_db = -100.0f;
    if (params->max_db) *params->max_db = 0.0f;
    if (params->inc_db) *params->inc_db = 1.0f;
    params->result = S_OK;
    return STATUS_SUCCESS;
}

/* Make the server-side default follow mmdevapi's default. Writes the
 * "default" metadata object: default.configured.audio.sink is what
 * WirePlumber persists/acts on (it then updates default.audio.sink);
 * we also write the plain key for metadata-only setups. */
static NTSTATUS pw_set_default_endpoint(void *args)
{
    struct set_default_endpoint_params *params = args;
    struct pw_metadata *meta;
    const char *key_cfg, *key_live;
    char json[600];
    size_t i, o;

    /* Empty device = revert to system default: nothing to push; WirePlumber
     * keeps its own choice. */
    if (!params->device || !params->device[0]) {
        params->result = S_OK;
        return STATUS_SUCCESS;
    }

    if (params->flow == eRender) {
        key_cfg  = "default.configured.audio.sink";
        key_live = "default.audio.sink";
    } else {
        key_cfg  = "default.configured.audio.source";
        key_live = "default.audio.source";
    }

    /* Minimal JSON string escape of the node name. */
    o = (size_t)snprintf(json, sizeof(json), "{\"name\":\"");
    for (i = 0; params->device[i] && o < sizeof(json) - 3; i++) {
        char c = params->device[i];
        if (c == '"' || c == '\\') {
            if (o >= sizeof(json) - 4) break;
            json[o++] = '\\';
        }
        json[o++] = c;
    }
    json[o++] = '"'; json[o++] = '}'; json[o] = 0;

    pw_lock();
    if (!pw_reg || !g_metadata_id) {
        pw_unlock();
        params->result = E_NOTIMPL;   /* no default metadata object (no session manager) */
        return STATUS_SUCCESS;
    }

    meta = pw_registry_bind(pw_reg, g_metadata_id, PW_TYPE_INTERFACE_Metadata,
                            PW_VERSION_METADATA, 0);
    if (!meta) {
        pw_unlock();
        params->result = E_FAIL;
        return STATUS_SUCCESS;
    }

    pw_metadata_set_property(meta, 0, key_cfg,  "Spa:String:JSON", json);
    pw_metadata_set_property(meta, 0, key_live, "Spa:String:JSON", json);
    core_roundtrip();
    pw_proxy_destroy((struct pw_proxy *)meta);
    pw_unlock();

    params->result = S_OK;
    return STATUS_SUCCESS;
}

/* ------------------------------------------------------------------ *
 *  Dispatch table (order MUST match enum unix_funcs)                  *
 * ------------------------------------------------------------------ */

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    pw_process_attach,
    pw_process_detach,
    pw_main_loop,
    pw_get_endpoint_ids,
    pw_create_stream,
    pw_release_stream,
    pw_start,
    pw_stop,
    pw_reset,
    pw_timer_loop,
    pw_get_render_buffer,
    pw_release_render_buffer,
    pw_get_capture_buffer,
    pw_release_capture_buffer,
    pw_is_format_supported,
    pw_get_loopback_capture_device,
    pw_get_mix_format,
    pw_get_device_period,
    pw_get_buffer_size,
    pw_get_latency,
    pw_get_current_padding,
    pw_get_next_packet_size,
    pw_get_frequency,
    pw_get_position,
    pw_set_volumes,
    pw_set_event_handle,
    pw_set_sample_rate,
    pw_test_connect,
    pw_is_started,
    pw_get_prop_value,
    pw_midi_get_driver,
    pw_not_implemented,   /* midi_init        */
    pw_not_implemented,   /* midi_release     */
    pw_not_implemented,   /* midi_out_message */
    pw_not_implemented,   /* midi_in_message  */
    pw_not_implemented,   /* midi_notify_wait */
    pw_not_implemented,   /* aux_message      */
    pw_set_endpoint_volume,
    pw_get_endpoint_volume,
    pw_get_endpoint_volume_range,
    pw_set_default_endpoint,
};

C_ASSERT(ARRAYSIZE(__wine_unix_call_funcs) == funcs_count);

/* ------------------------------------------------------------------ *
 *  WoW64 thunks                                                       *
 *                                                                     *
 *  Pe-side params are identical to pulse (shared mmdevapi ABI). The   *
 *  thunks below marshal the pointer-bearing structs; scalar-only      *
 *  calls reuse the 64-bit implementation directly.                    *
 * ------------------------------------------------------------------ */

#ifdef _WIN64

typedef UINT PTR32;

static NTSTATUS pw_wow64_main_loop(void *args)
{
    struct { PTR32 event; } *p32 = args;
    struct main_loop_params params = { .event = ULongToHandle(p32->event) };
    return pw_main_loop(&params);
}

static NTSTATUS pw_wow64_get_endpoint_ids(void *args)
{
    struct {
        EDataFlow flow;
        PTR32 endpoints;
        unsigned int size;
        HRESULT result;
        unsigned int num;
        unsigned int default_idx;
    } *p32 = args;
    struct get_endpoint_ids_params params =
    {
        .flow = p32->flow,
        .endpoints = ULongToPtr(p32->endpoints),
        .size = p32->size,
    };
    pw_get_endpoint_ids(&params);
    p32->size = params.size;
    p32->result = params.result;
    p32->num = params.num;
    p32->default_idx = params.default_idx;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_create_stream(void *args)
{
    struct {
        PTR32 name;
        PTR32 device;
        EDataFlow flow;
        AUDCLNT_SHAREMODE share;
        DWORD flags;
        REFERENCE_TIME duration;
        REFERENCE_TIME period;
        PTR32 fmt;
        HRESULT result;
        PTR32 channel_count;
        PTR32 stream;
    } *p32 = args;
    struct create_stream_params params =
    {
        .name = ULongToPtr(p32->name),
        .device = ULongToPtr(p32->device),
        .flow = p32->flow,
        .share = p32->share,
        .flags = p32->flags,
        .duration = p32->duration,
        .period = p32->period,
        .fmt = ULongToPtr(p32->fmt),
        .channel_count = ULongToPtr(p32->channel_count),
        .stream = ULongToPtr(p32->stream),
    };
    pw_create_stream(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_release_stream(void *args)
{
    struct {
        stream_handle stream;
        PTR32 timer_thread;
        HRESULT result;
    } *p32 = args;
    struct release_stream_params params =
    {
        .stream = p32->stream,
        .timer_thread = ULongToHandle(p32->timer_thread),
    };
    pw_release_stream(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_render_buffer(void *args)
{
    struct {
        stream_handle stream;
        UINT32 frames;
        HRESULT result;
        PTR32 data;
    } *p32 = args;
    BYTE *data = NULL;
    struct get_render_buffer_params params =
    {
        .stream = p32->stream,
        .frames = p32->frames,
        .data = &data,
    };
    pw_get_render_buffer(&params);
    p32->result = params.result;
    *(unsigned int *)ULongToPtr(p32->data) = PtrToUlong(data);
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_capture_buffer(void *args)
{
    struct {
        stream_handle stream;
        HRESULT result;
        PTR32 data;
        PTR32 frames;
        PTR32 flags;
        PTR32 devpos;
        PTR32 qpcpos;
    } *p32 = args;
    BYTE *data = NULL;
    struct get_capture_buffer_params params =
    {
        .stream = p32->stream,
        .data = &data,
        .frames = ULongToPtr(p32->frames),
        .flags = ULongToPtr(p32->flags),
        .devpos = ULongToPtr(p32->devpos),
        .qpcpos = ULongToPtr(p32->qpcpos),
    };
    pw_get_capture_buffer(&params);
    p32->result = params.result;
    *(unsigned int *)ULongToPtr(p32->data) = PtrToUlong(data);
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_is_format_supported(void *args)
{
    struct {
        PTR32 device;
        EDataFlow flow;
        AUDCLNT_SHAREMODE share;
        PTR32 fmt_in;
        HRESULT result;
    } *p32 = args;
    struct is_format_supported_params params =
    {
        .device = ULongToPtr(p32->device),
        .flow = p32->flow,
        .share = p32->share,
        .fmt_in = ULongToPtr(p32->fmt_in),
    };
    pw_is_format_supported(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_loopback_capture_device(void *args)
{
    struct {
        PTR32 name;
        PTR32 device;
        PTR32 ret_device;
        UINT32 ret_device_len;
        HRESULT result;
    } *p32 = args;
    struct get_loopback_capture_device_params params =
    {
        .name = ULongToPtr(p32->name),
        .device = ULongToPtr(p32->device),
        .ret_device = ULongToPtr(p32->ret_device),
        .ret_device_len = p32->ret_device_len,
    };
    pw_get_loopback_capture_device(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_mix_format(void *args)
{
    struct {
        PTR32 device;
        EDataFlow flow;
        PTR32 fmt;
        HRESULT result;
    } *p32 = args;
    struct get_mix_format_params params =
    {
        .device = ULongToPtr(p32->device),
        .flow = p32->flow,
        .fmt = ULongToPtr(p32->fmt),
    };
    pw_get_mix_format(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_device_period(void *args)
{
    struct {
        PTR32 device;
        EDataFlow flow;
        HRESULT result;
        PTR32 def_period;
        PTR32 min_period;
    } *p32 = args;
    struct get_device_period_params params =
    {
        .device = ULongToPtr(p32->device),
        .flow = p32->flow,
        .def_period = ULongToPtr(p32->def_period),
        .min_period = ULongToPtr(p32->min_period),
    };
    pw_get_device_period(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_buffer_size(void *args)
{
    struct { stream_handle stream; HRESULT result; PTR32 frames; } *p32 = args;
    struct get_buffer_size_params params = { .stream = p32->stream, .frames = ULongToPtr(p32->frames) };
    pw_get_buffer_size(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_latency(void *args)
{
    struct { stream_handle stream; HRESULT result; PTR32 latency; } *p32 = args;
    struct get_latency_params params = { .stream = p32->stream, .latency = ULongToPtr(p32->latency) };
    pw_get_latency(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_current_padding(void *args)
{
    struct { stream_handle stream; HRESULT result; PTR32 padding; } *p32 = args;
    struct get_current_padding_params params = { .stream = p32->stream, .padding = ULongToPtr(p32->padding) };
    pw_get_current_padding(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_next_packet_size(void *args)
{
    struct { stream_handle stream; HRESULT result; PTR32 frames; } *p32 = args;
    struct get_next_packet_size_params params = { .stream = p32->stream, .frames = ULongToPtr(p32->frames) };
    pw_get_next_packet_size(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_frequency(void *args)
{
    struct { stream_handle stream; HRESULT result; PTR32 freq; } *p32 = args;
    struct get_frequency_params params = { .stream = p32->stream, .freq = ULongToPtr(p32->freq) };
    pw_get_frequency(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_position(void *args)
{
    struct {
        stream_handle stream;
        BOOL device;
        HRESULT result;
        PTR32 pos;
        PTR32 qpctime;
    } *p32 = args;
    struct get_position_params params =
    {
        .stream = p32->stream,
        .device = p32->device,
        .pos = ULongToPtr(p32->pos),
        .qpctime = ULongToPtr(p32->qpctime),
    };
    pw_get_position(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_set_volumes(void *args)
{
    struct {
        stream_handle stream;
        float master_volume;
        PTR32 volumes;
        PTR32 session_volumes;
    } *p32 = args;
    struct set_volumes_params params =
    {
        .stream = p32->stream,
        .master_volume = p32->master_volume,
        .volumes = ULongToPtr(p32->volumes),
        .session_volumes = ULongToPtr(p32->session_volumes),
    };
    return pw_set_volumes(&params);
}

static NTSTATUS pw_wow64_set_event_handle(void *args)
{
    struct { stream_handle stream; PTR32 event; HRESULT result; } *p32 = args;
    struct set_event_handle_params params = { .stream = p32->stream, .event = ULongToHandle(p32->event) };
    pw_set_event_handle(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_test_connect(void *args)
{
    struct { PTR32 name; enum driver_priority priority; } *p32 = args;
    struct test_connect_params params = { .name = ULongToPtr(p32->name) };
    pw_test_connect(&params);
    p32->priority = params.priority;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_prop_value(void *args)
{
    struct propvariant32 { WORD vt; WORD pad1, pad2, pad3; union { ULONG ulVal; PTR32 ptr; }; } *value32;
    struct {
        PTR32 device;
        EDataFlow flow;
        PTR32 guid;
        PTR32 prop;
        HRESULT result;
        PTR32 value;
        PTR32 buffer;
        PTR32 buffer_size;
    } *p32 = args;
    PROPVARIANT value;
    struct get_prop_value_params params =
    {
        .device = ULongToPtr(p32->device),
        .flow = p32->flow,
        .guid = ULongToPtr(p32->guid),
        .prop = ULongToPtr(p32->prop),
        .value = &value,
        .buffer = ULongToPtr(p32->buffer),
        .buffer_size = ULongToPtr(p32->buffer_size),
    };
    pw_get_prop_value(&params);
    p32->result = params.result;
    if (SUCCEEDED(params.result)) {
        value32 = ULongToPtr(p32->value);
        value32->vt = value.vt;
        switch (value.vt) {
        case VT_UI4:    value32->ulVal = value.ulVal; break;
        case VT_LPWSTR: value32->ptr = p32->buffer;   break;
        default: FIXME("Unhandled vt %04x\n", value.vt);
        }
    }
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_set_endpoint_volume(void *args)
{
    struct
    {
        PTR32 device;
        EDataFlow flow;
        float level;
        int mute;
        HRESULT result;
    } *p32 = args;
    struct set_endpoint_volume_params params =
    {
        .device = ULongToPtr(p32->device),
        .flow = p32->flow,
        .level = p32->level,
        .mute = p32->mute,
    };
    pw_set_endpoint_volume(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_endpoint_volume(void *args)
{
    struct
    {
        PTR32 device;
        EDataFlow flow;
        PTR32 level;
        PTR32 mute;
        HRESULT result;
    } *p32 = args;
    struct get_endpoint_volume_params params =
    {
        .device = ULongToPtr(p32->device),
        .flow = p32->flow,
        .level = ULongToPtr(p32->level),
        .mute = ULongToPtr(p32->mute),
    };
    pw_get_endpoint_volume(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_set_default_endpoint(void *args)
{
    struct
    {
        PTR32 device;
        EDataFlow flow;
        HRESULT result;
    } *p32 = args;
    struct set_default_endpoint_params params =
    {
        .device = ULongToPtr(p32->device),
        .flow = p32->flow,
    };
    pw_set_default_endpoint(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

static NTSTATUS pw_wow64_get_endpoint_volume_range(void *args)
{
    struct
    {
        PTR32 device;
        EDataFlow flow;
        PTR32 min_db;
        PTR32 max_db;
        PTR32 inc_db;
        HRESULT result;
    } *p32 = args;
    struct get_endpoint_volume_range_params params =
    {
        .device = ULongToPtr(p32->device),
        .flow = p32->flow,
        .min_db = ULongToPtr(p32->min_db),
        .max_db = ULongToPtr(p32->max_db),
        .inc_db = ULongToPtr(p32->inc_db),
    };
    pw_get_endpoint_volume_range(&params);
    p32->result = params.result;
    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    pw_process_attach,
    pw_process_detach,
    pw_wow64_main_loop,
    pw_wow64_get_endpoint_ids,
    pw_wow64_create_stream,
    pw_wow64_release_stream,
    pw_start,
    pw_stop,
    pw_reset,
    pw_timer_loop,
    pw_wow64_get_render_buffer,
    pw_release_render_buffer,
    pw_wow64_get_capture_buffer,
    pw_release_capture_buffer,
    pw_wow64_is_format_supported,
    pw_wow64_get_loopback_capture_device,
    pw_wow64_get_mix_format,
    pw_wow64_get_device_period,
    pw_wow64_get_buffer_size,
    pw_wow64_get_latency,
    pw_wow64_get_current_padding,
    pw_wow64_get_next_packet_size,
    pw_wow64_get_frequency,
    pw_wow64_get_position,
    pw_wow64_set_volumes,
    pw_wow64_set_event_handle,
    pw_set_sample_rate,
    pw_wow64_test_connect,
    pw_is_started,
    pw_wow64_get_prop_value,
    pw_midi_get_driver,
    pw_not_implemented,
    pw_not_implemented,
    pw_not_implemented,
    pw_not_implemented,
    pw_not_implemented,
    pw_not_implemented,
    pw_wow64_set_endpoint_volume,
    pw_wow64_get_endpoint_volume,
    pw_wow64_get_endpoint_volume_range,
    pw_wow64_set_default_endpoint,
};

C_ASSERT(ARRAYSIZE(__wine_unix_call_wow64_funcs) == funcs_count);

#endif /* _WIN64 */
