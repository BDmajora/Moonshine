/* appmixer.h — private per-application playback mixer.
 *
 * Implemented by mmdevapi, consumed by sndvol.exe. Surfaces the PipeWire
 * playback graph (every output stream) coalesced into one row per
 * application. Obtained via IMMDevice::Activate(&IID_IAppMixer, ...).
 */
#ifndef __APPMIXER_H
#define __APPMIXER_H

#define APPMIXER_NAME_LEN 64

typedef struct appmixer_entry
{
    unsigned int id;        /* stable app-group id; pass back to Set* */
    float        volume;    /* 0.0 .. 1.0 */
    int          mute;      /* 0/1 */
    WCHAR        name[APPMIXER_NAME_LEN];
} appmixer_entry;

#ifdef __cplusplus
extern "C" {
    #endif
    extern const GUID IID_IAppMixer;
    #ifdef __cplusplus
}
#endif

typedef struct IAppMixer IAppMixer;

typedef struct IAppMixerVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IAppMixer *, REFIID, void **);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IAppMixer *);
    ULONG   (STDMETHODCALLTYPE *Release)(IAppMixer *);
    HRESULT (STDMETHODCALLTYPE *GetStreams)(IAppMixer *, UINT max,
                                            appmixer_entry *entries, UINT *count);
    HRESULT (STDMETHODCALLTYPE *GetVolume)(IAppMixer *, UINT id,
                                           float *volume, BOOL *mute);
    HRESULT (STDMETHODCALLTYPE *SetVolume)(IAppMixer *, UINT id, float volume);
    HRESULT (STDMETHODCALLTYPE *SetMute)(IAppMixer *, UINT id, BOOL mute);
} IAppMixerVtbl;

struct IAppMixer { const IAppMixerVtbl *lpVtbl; };

#ifdef COBJMACROS
#define IAppMixer_QueryInterface(t,a,b) (t)->lpVtbl->QueryInterface(t,a,b)
#define IAppMixer_AddRef(t)             (t)->lpVtbl->AddRef(t)
#define IAppMixer_Release(t)            (t)->lpVtbl->Release(t)
#define IAppMixer_GetStreams(t,a,b,c)   (t)->lpVtbl->GetStreams(t,a,b,c)
#define IAppMixer_GetVolume(t,a,b,c)    (t)->lpVtbl->GetVolume(t,a,b,c)
#define IAppMixer_SetVolume(t,a,b)      (t)->lpVtbl->SetVolume(t,a,b)
#define IAppMixer_SetMute(t,a,b)        (t)->lpVtbl->SetMute(t,a,b)
#endif

#endif /* __APPMIXER_H */
