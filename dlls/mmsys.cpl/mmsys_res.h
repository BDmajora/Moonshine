/* mmsys.cpl — resource IDs. RC-safe: defines only (included by mmsys.rc
 * and, via mmsys_private.h, by all C sources). */

#ifndef __MMSYS_RES_H
#define __MMSYS_RES_H

#define ICO_SOUND               100

#define IDS_CPL_NAME            1
#define IDS_CPL_INFO            2

/* Top-level Sound dialog pages */
#define IDD_PLAYBACK            200
#define IDD_RECORDING           201
#define IDD_SOUNDS              202
#define IDD_COMMS               203

/* Speakers/endpoint Properties pages */
#define IDD_SPK_GENERAL         210
#define IDD_SPK_LEVELS          211
#define IDD_SPK_ENH             212
#define IDD_SPK_ADV             213

/* Playback / Recording controls */
#define IDC_DEVICE_LIST         1000
#define IDC_SET_DEFAULT         1001
#define IDC_PROPERTIES          1002
#define IDC_CONFIGURE           1003

/* Sounds controls */
#define IDC_SCHEME_COMBO        1010
#define IDC_EVENT_LIST          1011
#define IDC_SOUND_PATH          1012
#define IDC_BROWSE              1013
#define IDC_TEST                1014

/* Communications controls */
#define IDC_COMM_MUTE           1020
#define IDC_COMM_80             1021
#define IDC_COMM_50             1022
#define IDC_COMM_NOTHING        1023

/* General tab */
#define IDC_GEN_NAME            1030
#define IDC_GEN_CONTROLLER      1031
#define IDC_GEN_JACK            1032
#define IDC_GEN_USAGE           1033

/* Levels tab */
#define IDC_LVL_SLIDER          1040
#define IDC_LVL_VALUE           1041
#define IDC_LVL_MUTE            1042
#define IDC_LVL_BALANCE         1043

/* Enhancements tab */
#define IDC_ENH_DISABLE_ALL     1050
#define IDC_ENH_BASS            1051
#define IDC_ENH_SURROUND        1052
#define IDC_ENH_LOUDNESS        1053
#define IDC_ENH_ROOM            1054

/* Advanced tab */
#define IDC_ADV_FORMAT          1060
#define IDC_ADV_TEST            1061
#define IDC_ADV_EXCL_ALLOW      1062
#define IDC_ADV_EXCL_PRIO       1063
#define IDC_ADV_RESTORE         1064

#endif /* __MMSYS_RES_H */
