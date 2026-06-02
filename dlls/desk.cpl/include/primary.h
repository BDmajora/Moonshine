/* desk.cpl — "Make this my main display" (primary output selection). */

#ifndef DESK_PRIMARY_H
#define DESK_PRIMARY_H

#include "desk_private.h"

/* TRUE if this output is currently the primary (its top-left is the layout
 * origin, 0,0). */
BOOL output_is_primary(const struct wlr_output_info *out);

/* Re-anchor the layout so `sel` becomes the primary output (origin 0,0),
 * preserving the relative arrangement of the others.  Returns FALSE if any
 * per-output position change failed. */
BOOL make_output_primary(const struct wlr_output_info *sel);

#endif /* DESK_PRIMARY_H */