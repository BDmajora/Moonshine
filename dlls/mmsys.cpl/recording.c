/* mmsys.cpl — Recording page. Same engine as Playback, eCapture flow.
 * Live peak meters (IAudioMeterInformation) are a later stretch goal. */

#include "mmsys_private.h"

INT_PTR CALLBACK recording_dialog_proc(HWND hwnd, UINT msg,
                                       WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        endpoint_page_init(hwnd, eCapture);
        return TRUE;

    case WM_COMMAND:
        endpoint_page_command(hwnd, eCapture, wparam);
        return TRUE;

    case WM_NOTIFY:
        endpoint_page_notify(hwnd, eCapture, lparam);
        return TRUE;

    case WM_DESTROY:
        endpoint_page_destroy(hwnd);
        return TRUE;
    }
    return FALSE;
}
