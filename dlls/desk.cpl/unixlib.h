/*
 * desk.cpl — shared interface between PE and Unix sides.
 *
 * Copyright 2025 YetiOS contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef DESK_UNIXLIB_H
#define DESK_UNIXLIB_H

/* limits ----------------------------------------------------------------- */
#define MAX_WLR_OUTPUTS  8
#define MAX_WLR_MODES    64

/* unix call function codes ----------------------------------------------- */
enum desk_unix_funcs
{
    unix_wlr_enumerate,
    unix_wlr_apply,
    unix_funcs_count
};

/* transform / orientation (matches wlr-randr order) ---------------------- */
#define WLR_TRANSFORM_NORMAL  0   /* Landscape                              */
#define WLR_TRANSFORM_90      1   /* Portrait                               */
#define WLR_TRANSFORM_180     2   /* Landscape (flipped)                    */
#define WLR_TRANSFORM_270     3   /* Portrait (flipped)                     */

/* apply flags — set the bits for the fields you want wlr-randr to change - */
#define WLR_APPLY_MODE        0x01   /* width + height                      */
#define WLR_APPLY_REFRESH     0x02   /* refresh_mhz                         */
#define WLR_APPLY_TRANSFORM   0x04   /* transform                           */
#define WLR_APPLY_VRR         0x08   /* adaptive_sync                       */

/* data shared between PE and Unix sides ---------------------------------- */

struct wlr_mode_info
{
    unsigned int width;
    unsigned int height;
    unsigned int refresh_mhz;    /* millihertz — 60000 = 60.000 Hz         */
    int          preferred;
    int          current;
};

struct wlr_output_info
{
    char         name[64];       /* connector name, e.g. "eDP-1"           */
    char         description[256];
    int          enabled;
    int          pos_x;
    int          pos_y;
    unsigned int transform;      /* WLR_TRANSFORM_*                        */
    int          adaptive_sync;  /* 0 = disabled, 1 = enabled              */
    unsigned int num_modes;
    struct wlr_mode_info modes[MAX_WLR_MODES];
};

struct wlr_enumerate_params
{
    unsigned int           num_outputs;
    struct wlr_output_info outputs[MAX_WLR_OUTPUTS];
};

struct wlr_apply_params
{
    char         output_name[64];
    unsigned int flags;          /* WLR_APPLY_* bits                        */
    unsigned int width;
    unsigned int height;
    unsigned int refresh_mhz;
    unsigned int transform;      /* WLR_TRANSFORM_*                        */
    int          adaptive_sync;  /* 0 = disable, 1 = enable                */
};

#endif /* DESK_UNIXLIB_H */