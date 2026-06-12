/* sndvol.exe — audio session enumeration.
 *
 * Snapshots the active audio sessions on an endpoint through
 * IAudioSessionManager2 -> IAudioSessionEnumerator. Each row carries the
 * ISimpleAudioVolume that the mixer sliders drive (mmdevapi applies it as
 * the per-stream session gain, which winepipewire.drv folds into
 * apply_volume on the stream — independent of the endpoint master).
 *
 * Labels follow the Windows mixer convention:
 *   - the system-sounds session is labelled "System Sounds"
 *   - otherwise IAudioSessionControl::GetDisplayName if the app set one
 *   - otherwise the owning process's image basename without extension
 */

#include "sndvol_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(sndvol);

static void label_from_pid(DWORD pid, WCHAR *label, size_t count)
{
    HANDLE proc;
    WCHAR path[MAX_PATH];
    DWORD len = ARRAY_SIZE(path);
    const WCHAR *base, *p;

    swprintf(label, count, L"Process %lu", pid);   /* fallback */

    proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) return;

    if (QueryFullProcessImageNameW(proc, 0, path, &len))
    {
        base = path;
        for (p = path; *p; p++)
            if (*p == '\\' || *p == '/') base = p + 1;
        lstrcpynW(label, base, count);
        /* strip ".exe" */
        {
            int l = lstrlenW(label);
            if (l > 4 && !lstrcmpiW(label + l - 4, L".exe"))
                label[l - 4] = 0;
        }
        /* capitalize first letter, mixer-style */
        if (label[0] >= 'a' && label[0] <= 'z')
            label[0] -= 32;
    }
    CloseHandle(proc);
}

int snapshot_sessions(IMMDevice *dev, struct session_info **out)
{
    IAudioSessionManager2 *mgr = NULL;
    IAudioSessionEnumerator *list = NULL;
    struct session_info *sessions = NULL;
    int i, total = 0, count = 0;
    HRESULT hr;

    *out = NULL;

    hr = IMMDevice_Activate(dev, &IID_IAudioSessionManager2,
                            CLSCTX_INPROC_SERVER, NULL, (void **)&mgr);
    if (FAILED(hr))
    {
        WARN("IAudioSessionManager2 activation failed: %08lx\n", hr);
        return -1;
    }

    hr = IAudioSessionManager2_GetSessionEnumerator(mgr, &list);
    IAudioSessionManager2_Release(mgr);
    if (FAILED(hr))
    {
        WARN("GetSessionEnumerator failed: %08lx\n", hr);
        return -1;
    }

    IAudioSessionEnumerator_GetCount(list, &total);
    if (total > 0)
    {
        sessions = calloc(total, sizeof(*sessions));
        if (!sessions)
        {
            IAudioSessionEnumerator_Release(list);
            return -1;
        }
    }

    for (i = 0; i < total; i++)
    {
        IAudioSessionControl *ctl = NULL;
        IAudioSessionControl2 *ctl2 = NULL;
        ISimpleAudioVolume *vol = NULL;
        struct session_info *s = &sessions[count];
        AudioSessionState state = AudioSessionStateExpired;
        WCHAR *name = NULL;

        if (FAILED(IAudioSessionEnumerator_GetSession(list, i, &ctl)))
            continue;

        IAudioSessionControl_GetState(ctl, &state);
        if (state == AudioSessionStateExpired)
        {
            IAudioSessionControl_Release(ctl);
            continue;
        }

        if (FAILED(IAudioSessionControl_QueryInterface(ctl,
                &IID_IAudioSessionControl2, (void **)&ctl2)) ||
            FAILED(IAudioSessionControl_QueryInterface(ctl,
                &IID_ISimpleAudioVolume, (void **)&vol)))
        {
            if (ctl2) IAudioSessionControl2_Release(ctl2);
            IAudioSessionControl_Release(ctl);
            continue;
        }
        IAudioSessionControl_Release(ctl);

        s->control = ctl2;
        s->volume = vol;
        IAudioSessionControl2_GetProcessId(ctl2, &s->pid);
        s->system_sounds =
            (IAudioSessionControl2_IsSystemSoundsSession(ctl2) == S_OK);

        if (s->system_sounds)
            lstrcpynW(s->label, L"System Sounds", ARRAY_SIZE(s->label));
        else if (SUCCEEDED(IAudioSessionControl2_GetDisplayName(ctl2, &name)) &&
                 name && name[0])
        {
            lstrcpynW(s->label, name, ARRAY_SIZE(s->label));
            CoTaskMemFree(name);
        }
        else
        {
            if (name) CoTaskMemFree(name);
            label_from_pid(s->pid, s->label, ARRAY_SIZE(s->label));
        }

        count++;
    }

    IAudioSessionEnumerator_Release(list);

    *out = sessions;
    return count;
}

void free_sessions(struct session_info *sessions, int count)
{
    int i;
    if (!sessions) return;
    for (i = 0; i < count; i++)
    {
        if (sessions[i].volume)  ISimpleAudioVolume_Release(sessions[i].volume);
        if (sessions[i].control) IAudioSessionControl2_Release(sessions[i].control);
    }
    free(sessions);
}

BOOL sessions_equal(const struct session_info *a, int na,
                    const struct session_info *b, int nb)
{
    int i;
    if (na != nb) return FALSE;
    for (i = 0; i < na; i++)
        if (a[i].pid != b[i].pid || a[i].system_sounds != b[i].system_sounds)
            return FALSE;
    return TRUE;
}
