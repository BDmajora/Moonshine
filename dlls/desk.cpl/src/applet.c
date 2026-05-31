/* desk.cpl — control-panel entry points, sheet bootstrap, window classes. */

#include "desk_private.h"
#include "main_page.h"
#include "preview.h"
#include "identify.h"

#include <cpl.h>
#include "ole2.h"

WINE_DEFAULT_DEBUG_CHANNEL(deskcpl);

HMODULE module;

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
            .hInstance   = module,
            .pszTemplate = MAKEINTRESOURCEW(IDD_DESKTOP),
            .pfnDlgProc  = desktop_dialog_proc,
        },
    };
    PROPSHEETHEADERW header =
    {
        .dwSize     = sizeof(PROPSHEETHEADERW),
        .dwFlags    = PSH_PROPSHEETPAGE | PSH_USEICONID | PSH_USECALLBACK,
        .hwndParent = parent,
        .hInstance  = module,
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

static void register_window_class(void)
{
    WNDCLASSW cls =
    {
        .hInstance     = module,
        .lpfnWndProc  = desktop_view_proc,
        .lpszClassName = L"DeskCplDesktop",
    };
    WNDCLASSW idcls =
    {
        .hInstance     = module,
        .lpfnWndProc  = identify_proc,
        .lpszClassName = L"DeskCplIdentify",
    };
    RegisterClassW(&cls);
    RegisterClassW(&idcls);
}

static void unregister_window_class(void)
{
    UnregisterClassW(L"DeskCplDesktop", module);
    UnregisterClassW(L"DeskCplIdentify", module);
}

LONG CALLBACK CPlApplet(HWND hwnd, UINT command, LPARAM param1, LPARAM param2)
{
    TRACE("hwnd %p, command %u, param1 %#Ix, param2 %#Ix\n",
          hwnd, command, param1, param2);

    switch (command)
    {
    case CPL_INIT:
        register_window_class();
        return TRUE;

    case CPL_GETCOUNT:
        return 1;

    case CPL_INQUIRE:
    {
        CPLINFO *info = (CPLINFO *)param2;
        info->idIcon = ICO_MAIN;
        info->idName = IDS_CPL_NAME;
        info->idInfo = IDS_CPL_INFO;
        info->lData  = 0;
        return TRUE;
    }

    case CPL_DBLCLK:
        create_property_sheets(hwnd);
        break;

    case CPL_STOP:
        unregister_window_class();
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