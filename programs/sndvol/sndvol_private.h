/* sndvol.exe — private header. */

#ifndef __SNDVOL_PRIVATE_H
#define __SNDVOL_PRIVATE_H

#define COBJMACROS

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "commctrl.h"

#include "ole2.h"
#include "mmdeviceapi.h"
#include "endpointvolume.h"
#include "audiopolicy.h"

#include "wine/debug.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/* ------------------------------------------------------------------ */
/* Application source snapshot (mixer.c)                              */
/* ------------------------------------------------------------------ */

#define SESSION_LABEL_LEN 64
#define MAX_SNAP 32

struct session_info
{
    ISimpleAudioVolume *volume;   /* synthetic; released by free_sessions */
    UINT id;                      /* coalesced application-group id */
    WCHAR label[SESSION_LABEL_LEN];
};

/* Snapshot the per-application playback rows on `dev`. Returns the row
 * count, or -1 on failure. Caller frees with free_sessions. */
int snapshot_sessions(IMMDevice *dev, struct session_info **out);

void free_sessions(struct session_info *sessions, int count);

/* TRUE if the two snapshots contain the same application set (compared by
 * group id), meaning the mixer columns can be reused. */
BOOL sessions_equal(const struct session_info *a, int na,
                    const struct session_info *b, int nb);

/* ------------------------------------------------------------------ */
/* Windows (sndvol.c)                                                 */
/* ------------------------------------------------------------------ */

extern HINSTANCE instance;

int show_flyout(void);
int show_mixer(void);

#endif /* __SNDVOL_PRIVATE_H */