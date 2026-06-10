/* mmsys.cpl — endpoint Properties, Advanced tab.
 *
 * Default-format combo round-trips PKEY_AudioEngine_DeviceFormat through the
 * endpoint property store (Phase 2: devenum seeds it once, client.c
 * GetMixFormat reads it back first, so the choice here is what shared-mode
 * apps mix at, and it survives relaunch/reboot). Test renders a short tone
 * through IAudioClient with the selected format — an end-to-end driver test.
 * Exclusive-mode checkboxes persist for future enforcement (PipeWire backend
 * rejects exclusive mode regardless). */

#include "mmsys_private.h"

#include "mmreg.h"
#include "ks.h"
#include "ksmedia.h"
#include "math.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmsyscpl);

#define EXCL_ALLOW 0x1
#define EXCL_PRIO  0x2

static const WCHAR excl_key[] = L"Software\\Wine\\Audio\\Exclusive";

static const struct
{
    WORD bits;
    DWORD rate;
    const WCHAR *quality;
}
format_table[] =
{
    { 16,  44100, L"CD Quality" },
    { 16,  48000, L"DVD Quality" },
    { 16,  96000, L"Studio Quality" },
    { 16, 192000, L"Studio Quality" },
    { 24,  44100, L"Studio Quality" },
    { 24,  48000, L"Studio Quality" },
    { 24,  96000, L"Studio Quality" },
    { 24, 192000, L"Studio Quality" },
};

struct adv_page
{
    IMMDevice *dev;       /* not owned */
    WCHAR *id;
};

static void build_format(unsigned int idx, WAVEFORMATEXTENSIBLE *fmt)
{
    memset(fmt, 0, sizeof(*fmt));
    fmt->Format.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
    fmt->Format.nChannels       = 2;
    fmt->Format.nSamplesPerSec  = format_table[idx].rate;
    fmt->Format.wBitsPerSample  = format_table[idx].bits;
    fmt->Format.nBlockAlign     = fmt->Format.nChannels * fmt->Format.wBitsPerSample / 8;
    fmt->Format.nAvgBytesPerSec = fmt->Format.nSamplesPerSec * fmt->Format.nBlockAlign;
    fmt->Format.cbSize          = sizeof(*fmt) - sizeof(WAVEFORMATEX);
    fmt->Samples.wValidBitsPerSample = fmt->Format.wBitsPerSample;
    fmt->dwChannelMask          = KSAUDIO_SPEAKER_STEREO;
    fmt->SubFormat              = KSDATAFORMAT_SUBTYPE_PCM;
}

static DWORD read_excl(const WCHAR *id)
{
    HKEY key;
    DWORD val = EXCL_ALLOW | EXCL_PRIO, size = sizeof(val);  /* Windows defaults */

    if (RegOpenKeyExW(HKEY_CURRENT_USER, excl_key, 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        RegQueryValueExW(key, id, NULL, NULL, (BYTE *)&val, &size);
        RegCloseKey(key);
    }
    return val;
}

static void write_excl(const WCHAR *id, DWORD val)
{
    HKEY key;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, excl_key, 0, NULL, 0, KEY_WRITE,
                        NULL, &key, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(key, id, 0, REG_DWORD, (const BYTE *)&val, sizeof(val));
        RegCloseKey(key);
    }
}

/* Match a stored WAVEFORMATEX against the table; -1 if no match. */
static int match_format(const WAVEFORMATEX *fmt)
{
    unsigned int i;
    WORD bits = fmt->wBitsPerSample;

    if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        fmt->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX))
        bits = ((const WAVEFORMATEXTENSIBLE *)fmt)->Samples.wValidBitsPerSample;

    for (i = 0; i < ARRAY_SIZE(format_table); i++)
        if (format_table[i].bits == bits && format_table[i].rate == fmt->nSamplesPerSec)
            return (int)i;
    return -1;
}

static int current_format_index(struct adv_page *page)
{
    IPropertyStore *store = NULL;
    PROPVARIANT pv;
    int idx = -1;

    PropVariantInit(&pv);
    if (SUCCEEDED(IMMDevice_OpenPropertyStore(page->dev, STGM_READ, &store)) &&
        SUCCEEDED(IPropertyStore_GetValue(store, &PKEY_AudioEngine_DeviceFormat, &pv)) &&
        pv.vt == VT_BLOB && pv.blob.pBlobData &&
        pv.blob.cbSize >= sizeof(WAVEFORMATEX))
        idx = match_format((const WAVEFORMATEX *)pv.blob.pBlobData);
    PropVariantClear(&pv);
    if (store) IPropertyStore_Release(store);
    return idx;
}

static HRESULT save_format(struct adv_page *page, unsigned int idx)
{
    IPropertyStore *store = NULL;
    WAVEFORMATEXTENSIBLE fmt;
    PROPVARIANT pv;
    HRESULT hr;

    build_format(idx, &fmt);

    hr = IMMDevice_OpenPropertyStore(page->dev, STGM_WRITE, &store);
    if (FAILED(hr)) return hr;

    pv.vt = VT_BLOB;
    pv.blob.cbSize = sizeof(fmt);
    pv.blob.pBlobData = (BYTE *)&fmt;
    hr = IPropertyStore_SetValue(store, &PKEY_AudioEngine_DeviceFormat, &pv);
    if (SUCCEEDED(hr)) IPropertyStore_Commit(store);
    IPropertyStore_Release(store);
    return hr;
}

/* Render ~0.5s of 440 Hz through the endpoint with the selected format.
 * Exercises the entire path: mmdevapi -> winepipewire.drv -> PipeWire. */
static void play_test_tone(HWND hwnd, struct adv_page *page, unsigned int idx)
{
    IAudioClient *client = NULL;
    IAudioRenderClient *render = NULL;
    WAVEFORMATEXTENSIBLE fmt;
    UINT32 frames = 0;
    BYTE *buf;
    HRESULT hr;

    build_format(idx, &fmt);

    hr = IMMDevice_Activate(page->dev, &IID_IAudioClient, CLSCTX_INPROC_SERVER,
                            NULL, (void **)&client);
    if (FAILED(hr)) goto fail;

    hr = IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, 0,
                                 10000000 /* 1s */, 0, &fmt.Format, NULL);
    if (FAILED(hr)) goto fail;

    hr = IAudioClient_GetService(client, &IID_IAudioRenderClient, (void **)&render);
    if (FAILED(hr)) goto fail;

    IAudioClient_GetBufferSize(client, &frames);
    if (frames > fmt.Format.nSamplesPerSec / 2)
        frames = fmt.Format.nSamplesPerSec / 2;   /* cap at 0.5s */

    hr = IAudioRenderClient_GetBuffer(render, frames, &buf);
    if (FAILED(hr)) goto fail;

    {
        UINT32 i;
        const double step = 2.0 * 3.14159265358979 * 440.0 / fmt.Format.nSamplesPerSec;

        for (i = 0; i < frames; i++)
        {
            double s = sin(step * i) * 0.25;

            if (fmt.Format.wBitsPerSample == 16)
            {
                SHORT v = (SHORT)(s * 32767.0);
                SHORT *p = (SHORT *)(buf + (size_t)i * fmt.Format.nBlockAlign);
                p[0] = v; p[1] = v;
            }
            else /* 24-bit packed */
            {
                LONG v = (LONG)(s * 8388607.0);
                BYTE *p = buf + (size_t)i * fmt.Format.nBlockAlign;
                p[0] = (BYTE)v; p[1] = (BYTE)(v >> 8); p[2] = (BYTE)(v >> 16);
                p[3] = p[0]; p[4] = p[1]; p[5] = p[2];
            }
        }
    }
    IAudioRenderClient_ReleaseBuffer(render, frames, 0);

    IAudioClient_Start(client);
    Sleep(600);
    IAudioClient_Stop(client);

    IAudioRenderClient_Release(render);
    IAudioClient_Release(client);
    return;

fail:
    if (render) IAudioRenderClient_Release(render);
    if (client) IAudioClient_Release(client);
    MessageBoxW(hwnd, L"The test tone could not be played with this format.",
                L"Sound", MB_OK | MB_ICONWARNING);
}

INT_PTR CALLBACK spk_adv_dialog_proc(HWND hwnd, UINT msg,
                                     WPARAM wparam, LPARAM lparam)
{
    struct adv_page *page = (struct adv_page *)GetWindowLongPtrW(hwnd, DWLP_USER);

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        PROPSHEETPAGEW *psp = (PROPSHEETPAGEW *)lparam;
        HWND combo;
        unsigned int i;
        int cur;
        DWORD excl;

        page = calloc(1, sizeof(*page));
        if (!page) return TRUE;
        page->dev = (IMMDevice *)psp->lParam;
        IMMDevice_GetId(page->dev, &page->id);
        SetWindowLongPtrW(hwnd, DWLP_USER, (LONG_PTR)page);

        combo = GetDlgItem(hwnd, IDC_ADV_FORMAT);
        for (i = 0; i < ARRAY_SIZE(format_table); i++)
        {
            WCHAR buf[96];
            swprintf(buf, ARRAY_SIZE(buf), L"%u bit, %lu Hz (%s)",
                     format_table[i].bits, format_table[i].rate,
                     format_table[i].quality);
            SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)buf);
        }

        cur = current_format_index(page);
        if (cur < 0) cur = 1;   /* 16/48000 — the driver's mix default */
        SendMessageW(combo, CB_SETCURSEL, cur, 0);

        excl = page->id ? read_excl(page->id) : (EXCL_ALLOW | EXCL_PRIO);
        CheckDlgButton(hwnd, IDC_ADV_EXCL_ALLOW, (excl & EXCL_ALLOW) ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_ADV_EXCL_PRIO,  (excl & EXCL_PRIO)  ? BST_CHECKED : BST_UNCHECKED);
        EnableWindow(GetDlgItem(hwnd, IDC_ADV_EXCL_PRIO), (excl & EXCL_ALLOW) != 0);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case IDC_ADV_FORMAT:
            if (HIWORD(wparam) == CBN_SELCHANGE)
                SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
            break;

        case IDC_ADV_TEST:
        {
            int sel = (int)SendDlgItemMessageW(hwnd, IDC_ADV_FORMAT, CB_GETCURSEL, 0, 0);
            if (page && sel >= 0)
                play_test_tone(hwnd, page, (unsigned int)sel);
            break;
        }

        case IDC_ADV_EXCL_ALLOW:
            EnableWindow(GetDlgItem(hwnd, IDC_ADV_EXCL_PRIO),
                         IsDlgButtonChecked(hwnd, IDC_ADV_EXCL_ALLOW) == BST_CHECKED);
            /* fall through */
        case IDC_ADV_EXCL_PRIO:
            SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
            break;

        case IDC_ADV_RESTORE:
        {
            HWND combo = GetDlgItem(hwnd, IDC_ADV_FORMAT);
            SendMessageW(combo, CB_SETCURSEL, 1, 0);    /* 16/48000 */
            CheckDlgButton(hwnd, IDC_ADV_EXCL_ALLOW, BST_CHECKED);
            CheckDlgButton(hwnd, IDC_ADV_EXCL_PRIO, BST_CHECKED);
            EnableWindow(GetDlgItem(hwnd, IDC_ADV_EXCL_PRIO), TRUE);
            SendMessageW(GetParent(hwnd), PSM_CHANGED, (WPARAM)hwnd, 0);
            break;
        }
        }
        return TRUE;

    case WM_NOTIFY:
    {
        NMHDR *nm = (NMHDR *)lparam;
        if (nm->code == PSN_APPLY && page)
        {
            int sel = (int)SendDlgItemMessageW(hwnd, IDC_ADV_FORMAT, CB_GETCURSEL, 0, 0);
            DWORD excl = 0;

            if (sel >= 0 && FAILED(save_format(page, (unsigned int)sel)))
            {
                MessageBoxW(hwnd, L"Failed to save the default format.",
                            L"Sound", MB_OK | MB_ICONWARNING);
                SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, PSNRET_INVALID);
                return TRUE;
            }

            if (IsDlgButtonChecked(hwnd, IDC_ADV_EXCL_ALLOW) == BST_CHECKED) excl |= EXCL_ALLOW;
            if (IsDlgButtonChecked(hwnd, IDC_ADV_EXCL_PRIO) == BST_CHECKED)  excl |= EXCL_PRIO;
            if (page->id) write_excl(page->id, excl);

            SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        break;
    }

    case WM_DESTROY:
        if (page)
        {
            CoTaskMemFree(page->id);
            free(page);
            SetWindowLongPtrW(hwnd, DWLP_USER, 0);
        }
        return TRUE;
    }
    return FALSE;
}
