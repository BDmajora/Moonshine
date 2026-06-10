/* mmsys.cpl — endpoint Properties sub-sheet (General/Levels/Enhancements/
 * Advanced). Mirrors desk.cpl's open_advanced_settings. The IMMDevice is
 * handed to every page through PROPSHEETPAGE.lParam; this function holds a
 * reference for the sheet's (modal) lifetime, pages must not release it. */

#include "mmsys_private.h"

#include "functiondiscoverykeys_devpkey.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmsyscpl);

void open_endpoint_properties(HWND parent, IMMDevice *dev)
{
    IPropertyStore *store = NULL;
    PROPVARIANT pv;
    WCHAR title[320];

    PropVariantInit(&pv);
    if (SUCCEEDED(IMMDevice_OpenPropertyStore(dev, STGM_READ, &store)) &&
        SUCCEEDED(IPropertyStore_GetValue(store, (const PROPERTYKEY *)&DEVPKEY_Device_FriendlyName, &pv)) &&
        pv.vt == VT_LPWSTR && pv.pwszVal)
        swprintf(title, ARRAY_SIZE(title), L"%s Properties", pv.pwszVal);
    else
        lstrcpyW(title, L"Speakers Properties");
    PropVariantClear(&pv);
    if (store) IPropertyStore_Release(store);

    IMMDevice_AddRef(dev);

    {
        PROPSHEETPAGEW pages[] =
        {
            {
                .dwSize      = sizeof(PROPSHEETPAGEW),
                .dwFlags     = PSP_USETITLE,
                .hInstance   = module,
                .pszTemplate = MAKEINTRESOURCEW(IDD_SPK_GENERAL),
                .pszTitle    = L"General",
                .pfnDlgProc  = spk_general_dialog_proc,
                .lParam      = (LPARAM)dev,
            },
            {
                .dwSize      = sizeof(PROPSHEETPAGEW),
                .dwFlags     = PSP_USETITLE,
                .hInstance   = module,
                .pszTemplate = MAKEINTRESOURCEW(IDD_SPK_LEVELS),
                .pszTitle    = L"Levels",
                .pfnDlgProc  = spk_levels_dialog_proc,
                .lParam      = (LPARAM)dev,
            },
            {
                .dwSize      = sizeof(PROPSHEETPAGEW),
                .dwFlags     = PSP_USETITLE,
                .hInstance   = module,
                .pszTemplate = MAKEINTRESOURCEW(IDD_SPK_ENH),
                .pszTitle    = L"Enhancements",
                .pfnDlgProc  = spk_enh_dialog_proc,
                .lParam      = (LPARAM)dev,
            },
            {
                .dwSize      = sizeof(PROPSHEETPAGEW),
                .dwFlags     = PSP_USETITLE,
                .hInstance   = module,
                .pszTemplate = MAKEINTRESOURCEW(IDD_SPK_ADV),
                .pszTitle    = L"Advanced",
                .pfnDlgProc  = spk_adv_dialog_proc,
                .lParam      = (LPARAM)dev,
            },
        };
        PROPSHEETHEADERW hdr =
        {
            .dwSize     = sizeof(PROPSHEETHEADERW),
            .dwFlags    = PSH_PROPSHEETPAGE,
            .hwndParent = parent,
            .hInstance  = module,
            .pszCaption = title,
            .nPages     = ARRAY_SIZE(pages),
            .ppsp       = pages,
        };

        PropertySheetW(&hdr);
    }

    IMMDevice_Release(dev);
}
