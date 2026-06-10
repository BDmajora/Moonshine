/* mmsys.cpl — private header. */

#ifndef __MMSYS_PRIVATE_H
#define __MMSYS_PRIVATE_H

#define COBJMACROS

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winuser.h"
#include "commctrl.h"
#include "prsht.h"

#include "ole2.h"
#include "mmdeviceapi.h"
#include "endpointvolume.h"
#include "audioclient.h"

#include "wine/debug.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

extern HMODULE module;

/* ------------------------------------------------------------------ */
/* Resource IDs                                                       */
/* ------------------------------------------------------------------ */

#include "mmsys_res.h"

/* ------------------------------------------------------------------ */
/* Cross-file entry points                                            */
/* ------------------------------------------------------------------ */

/* applet.c */
extern IMMDeviceEnumerator *create_enumerator(void);

/* playback.c — shared endpoint-list page engine, used by recording.c too */
extern void endpoint_page_init(HWND hwnd, EDataFlow flow);
extern void endpoint_page_refresh(HWND hwnd, EDataFlow flow);
extern void endpoint_page_destroy(HWND hwnd);
extern void endpoint_page_command(HWND hwnd, EDataFlow flow, WPARAM wparam);
extern void endpoint_page_notify(HWND hwnd, EDataFlow flow, LPARAM lparam);
extern INT_PTR CALLBACK playback_dialog_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

/* recording.c */
extern INT_PTR CALLBACK recording_dialog_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

/* sounds.c */
extern INT_PTR CALLBACK sounds_dialog_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

/* communications.c */
extern INT_PTR CALLBACK comms_dialog_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

/* speakers.c */
extern void open_endpoint_properties(HWND parent, IMMDevice *dev);

/* general.c / levels.c / enhancements.c / advanced_tab.c */
extern INT_PTR CALLBACK spk_general_dialog_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
extern INT_PTR CALLBACK spk_levels_dialog_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
extern INT_PTR CALLBACK spk_enh_dialog_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
extern INT_PTR CALLBACK spk_adv_dialog_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

#endif /* __MMSYS_PRIVATE_H */
