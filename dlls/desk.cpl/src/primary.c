/* desk.cpl — "Make this my main display" (primary output selection).
 *
 * Wayland/wlroots has no protocol-level "primary output" flag.  By the same
 * convention Windows uses — and the same one winewayland.drv already relies on
 * in display.c (output_info_cmp_primary_x_y) — the output whose top-left sits
 * at the layout origin (0,0) is the primary display.  So promoting an output
 * to primary means re-anchoring the whole layout so the chosen output lands at
 * (0,0), preserving every other output's *relative* arrangement:
 *
 *     new_pos = old_pos - chosen_origin
 *
 * which is exactly the coordinate shift described for Windows' "Make this my
 * main display": the selected monitor becomes (0,0) and everyone else is
 * recalculated relative to that new origin.
 *
 * The new positions are pushed to the compositor through wlr-randr --pos
 * (see unixlib.c).  Nothing else is persisted: the layout position is the
 * single source of truth.  winewayland.drv reads it back through the
 * zxdg_output logical position and stamps DISPLAY_DEVICE_PRIMARY_DEVICE on the
 * output at (0,0), so the primary flag reaches every Win32 app for free.
 */

#include "desk_private.h"
#include "primary.h"
#include "display_state.h"

WINE_DEFAULT_DEBUG_CHANNEL(deskcpl);

BOOL output_is_primary(const struct wlr_output_info *out)
{
    return out && out->enabled && out->pos_x == 0 && out->pos_y == 0;
}

BOOL make_output_primary(const struct wlr_output_info *sel)
{
    int          dx, dy;
    unsigned int i;
    BOOL         ok = TRUE;

    if (!sel) return FALSE;

    dx = sel->pos_x;
    dy = sel->pos_y;

    TRACE("Promoting %s to primary; shifting layout origin by (-%d,-%d)\n",
          sel->name, dx, dy);

    /* Re-anchor: subtract the chosen output's origin from every enabled
     * output, leaving the chosen one at (0,0). */
    for (i = 0; i < wlr_data.num_outputs; i++)
    {
        const struct wlr_output_info *o = &wlr_data.outputs[i];
        struct wlr_apply_params ap;

        if (!o->enabled) continue;

        memset(&ap, 0, sizeof(ap));
        lstrcpynA(ap.output_name, o->name, sizeof(ap.output_name));
        ap.flags = WLR_APPLY_POS;
        ap.pos_x = o->pos_x - dx;
        ap.pos_y = o->pos_y - dy;

        if (!apply_wlr(&ap))
        {
            WARN("Failed to position output %s at (%d,%d)\n",
                 o->name, ap.pos_x, ap.pos_y);
            ok = FALSE;
        }
    }

    /* Refresh the cache so callers see the new origin. */
    enumerate_outputs();
    return ok;
}