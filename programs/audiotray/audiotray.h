/* audiotray.exe — resource identifiers.
 *
 * Shared between audiotray.c and audiotray.rc. Kept to #define-only so wrc
 * can preprocess it without tripping over C declarations.
 */

#ifndef __AUDIOTRAY_H
#define __AUDIOTRAY_H

/* Tray context menu */
#define IDR_TRAYMENU     0x100
#define IDM_MIXER        0x101
#define IDM_PLAYBACK     0x102
#define IDM_RECORDING    0x103
#define IDM_SOUNDS       0x104

/* Tooltip format strings */
#define IDS_TIP_LEVEL    0x110
#define IDS_TIP_MUTED    0x111

#endif /* __AUDIOTRAY_H */
