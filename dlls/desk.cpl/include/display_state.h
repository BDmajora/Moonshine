/* desk.cpl — cached wlr-randr state and the Unix-side backend calls. */

#ifndef DESK_DISPLAY_STATE_H
#define DESK_DISPLAY_STATE_H

#include "desk_private.h"

/* Cached enumeration + the user's staged selections. */
extern struct wlr_enumerate_params wlr_data;
extern int          sel_output;        /* index or -1 */
extern unsigned int staged_w, staged_h;
extern unsigned int staged_transform;

BOOL ensure_unix(void);
BOOL enumerate_outputs(void);
BOOL apply_wlr(struct wlr_apply_params *ap);
const struct wlr_output_info *get_output(void);
int  current_bpp(void);

#endif /* DESK_DISPLAY_STATE_H */