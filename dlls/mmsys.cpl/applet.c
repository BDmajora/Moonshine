/* mmsys.cpl — control-panel entry points and Sound sheet bootstrap.
 * Structure mirrors desk.cpl/applet.c. */

#include <initguid.h>
#include "mmsys_private.h"

/* Instantiate every GUID/PROPERTYKEY the module references; all other
 * translation units get extern declarations from the same headers. */
#include "functiondiscoverykeys_devpkey.h"
#include "mmreg.h"
#include "ks.h"
#include "ksmedia.h"

#include <cpl.h>

WINE_DEFAULT_DEBUG_CHANNEL(mmsyscpl);

HMODULE module;

/* One-stop enumerator factory; caller releases. */
IMMDeviceEnumerator *create_enumerator(void)
{
    IMMDeviceEnumerator *devenum = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IMMDeviceEnumerator, (void **)&devenum);
    if (FAILED(hr))
    {
        ERR("Failed to create IMMDeviceEnumerator: %08lx\n", hr);
        return NULL;
    }
    return devenum;
}

static int CALLBACK property_sheet_callback(HWND hwnd, UINT msg, LPARAM lparam)
{
    TRACE("hwnd %p, msg %#x, lparam %#Ix\n", hwnd, msg, lparam);
    return 0;
}

static void create_property_sheets(HWND parent)
{
    INITCOMMONCONTROLSEX init =
    {
        .dwSize = sizeof(INITCOMMONCONTROLSEX),
        .dwICC  = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES,
    };
    PROPSHEETPAGEW pages[] =
    {
        {
            .dwSize      = sizeof(PROPSHEETPAGEW),
            .dwFlags     = PSP_USETITLE,
            .hInstance   = module,
            .pszTemplate = MAKEINTRESOURCEW(IDD_PLAYBACK),
            .pszTitle    = L"Playback",
            .pfnDlgProc  = playback_dialog_proc,
        },
        {
            .dwSize      = sizeof(PROPSHEETPAGEW),
            .dwFlags     = PSP_USETITLE,
            .hInstance   = module,
            .pszTemplate = MAKEINTRESOURCEW(IDD_RECORDING),
            .pszTitle    = L"Recording",
            .pfnDlgProc  = recording_dialog_proc,
        },
        {
            .dwSize      = sizeof(PROPSHEETPAGEW),
            .dwFlags     = PSP_USETITLE,
            .hInstance   = module,
            .pszTemplate = MAKEINTRESOURCEW(IDD_SOUNDS),
            .pszTitle    = L"Sounds",
            .pfnDlgProc  = sounds_dialog_proc,
        },
        {
            .dwSize      = sizeof(PROPSHEETPAGEW),
            .dwFlags     = PSP_USETITLE,
            .hInstance   = module,
            .pszTemplate = MAKEINTRESOURCEW(IDD_COMMS),
            .pszTitle    = L"Communications",
            .pfnDlgProc  = comms_dialog_proc,
        },
    };
    PROPSHEETHEADERW header =
    {
        .dwSize     = sizeof(PROPSHEETHEADERW),
        .dwFlags    = PSH_PROPSHEETPAGE | PSH_USEICONID | PSH_USECALLBACK,
        .hwndParent = parent,
        .hInstance  = module,
        .pszIcon    = MAKEINTRESOURCEW(ICO_SOUND),
        .pszCaption = MAKEINTRESOURCEW(IDS_CPL_NAME),
        .nPages     = ARRAY_SIZE(pages),
        .ppsp       = pages,
        .pfnCallback = property_sheet_callback,
    };
    ACTCTXW ctx =
    {
        .cbSize         = sizeof(ACTCTXW),
        .hModule        = module,
        .lpResourceName = MAKEINTRESOURCEW(124),
        .dwFlags        = ACTCTX_FLAG_HMODULE_VALID | ACTCTX_FLAG_RESOURCE_NAME_VALID,
    };
    ULONG_PTR cookie;
    HANDLE context;
    BOOL activated;

    OleInitialize(NULL);

    context = CreateActCtxW(&ctx);
    activated = (context != INVALID_HANDLE_VALUE) && ActivateActCtx(context, &cookie);

    InitCommonControlsEx(&init);
    PropertySheetW(&header);

    if (activated) DeactivateActCtx(0, cookie);
    ReleaseActCtx(context);
    OleUninitialize();
}

LONG CALLBACK CPlApplet(HWND hwnd, UINT command, LPARAM param1, LPARAM param2)
{
    TRACE("hwnd %p, command %u, param1 %#Ix, param2 %#Ix\n",
          hwnd, command, param1, param2);

    switch (command)
    {
    case CPL_INIT:
        return TRUE;

    case CPL_GETCOUNT:
        return 1;

    case CPL_INQUIRE:
    {
        CPLINFO *info = (CPLINFO *)param2;
        info->idIcon = ICO_SOUND;
        info->idName = IDS_CPL_NAME;
        info->idInfo = IDS_CPL_INFO;
        info->lData  = 0;
        return TRUE;
    }

    case CPL_DBLCLK:
        create_property_sheets(hwnd);
        break;

    case CPL_STOP:
        break;
    }

    return FALSE;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    TRACE("instance %p, reason %ld, reserved %p\n", instance, reason, reserved);

    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        module = instance;
    }

    return TRUE;
}
