/* desk.cpl — cached wlr-randr state and the Unix-side backend calls. */

#include "desk_private.h"
#include "display_state.h"

WINE_DEFAULT_DEBUG_CHANNEL(deskcpl);

#define UNIX_CALL(code, params) \
    __wine_unix_call(__wine_unixlib_handle, (code), (params))

/* Cached enumeration + the user's staged selections. */
struct wlr_enumerate_params wlr_data;
int          sel_output;
unsigned int staged_w, staged_h;
unsigned int staged_transform;

static BOOL unix_inited;

BOOL ensure_unix(void)
{
    if (!unix_inited)
    {
        if (__wine_init_unix_call())
        {
            ERR("Failed to initialise desk.cpl unix library.\n");
            return FALSE;
        }
        unix_inited = TRUE;
    }
    return TRUE;
}

const struct wlr_output_info *get_output(void)
{
    if (sel_output < 0 || sel_output >= (int)wlr_data.num_outputs)
        return NULL;
    return &wlr_data.outputs[sel_output];
}

BOOL enumerate_outputs(void)
{
    if (!ensure_unix()) return FALSE;
    return UNIX_CALL(unix_wlr_enumerate, &wlr_data) == 0;
}

/* Apply staged settings through wlr-randr; TRUE on success. */
BOOL apply_wlr(struct wlr_apply_params *ap)
{
    if (!ensure_unix()) return FALSE;
    return UNIX_CALL(unix_wlr_apply, ap) == 0;
}

/* Current desktop color depth, in bits per pixel. */
int current_bpp(void)
{
    HDC hdc = GetDC(NULL);
    int bpp = GetDeviceCaps(hdc, BITSPIXEL);
    ReleaseDC(NULL, hdc);
    return bpp;
}