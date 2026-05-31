/* desk.cpl — common includes shared by every PE-side source. */

#ifndef DESK_PRIVATE_H
#define DESK_PRIVATE_H

#include <stdarg.h>
#include <stddef.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include <commctrl.h>

#include "wine/debug.h"
#include "wine/unixlib.h"

#include "resource.h"
#include "unixlib.h"

/* Module handle, set in DllMain (applet.c). */
extern HMODULE module;

#endif /* DESK_PRIVATE_H */