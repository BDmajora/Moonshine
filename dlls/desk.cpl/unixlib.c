/*
 * desk.cpl — Unix-side library.
 *
 * Calls wlr-randr to enumerate Wayland outputs / modes and apply
 * resolution, refresh-rate, orientation, and VRR changes on
 * wlroots-based compositors (frostedglass).
 *
 * Copyright 2025 YetiOS contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "wine/unixlib.h"

#include "unixlib.h"

/* ------------------------------------------------------------------ */
/* helpers                                                            */
/* ------------------------------------------------------------------ */

static void strip_trailing(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' '))
        s[--len] = '\0';
}

static int safe_output_name(const char *name)
{
    for (const char *c = name; *c; c++)
        if (!isalnum((unsigned char)*c) && *c != '-' && *c != '_')
            return 0;
    return 1;
}

static unsigned int parse_transform(const char *val)
{
    while (isspace((unsigned char)*val)) val++;
    if (strncmp(val, "90",  2) == 0) return WLR_TRANSFORM_90;
    if (strncmp(val, "180", 3) == 0) return WLR_TRANSFORM_180;
    if (strncmp(val, "270", 3) == 0) return WLR_TRANSFORM_270;
    return WLR_TRANSFORM_NORMAL;
}

static const char *transform_str(unsigned int t)
{
    switch (t)
    {
    case WLR_TRANSFORM_90:  return "90";
    case WLR_TRANSFORM_180: return "180";
    case WLR_TRANSFORM_270: return "270";
    default:                return "normal";
    }
}

/* ------------------------------------------------------------------ */
/* wlr-randr output parser                                            */
/*                                                                    */
/* Expected format (wlr-randr >= 0.4):                                */
/*                                                                    */
/*   eDP-1 "Built-in display (eDP-1)"                                */
/*     Physical size: 340x190 mm                                      */
/*     Enabled: yes                                                   */
/*     Modes:                                                         */
/*       1920x1080 px, 60.003000 Hz (preferred, current)             */
/*       1280x720 px, 60.000000 Hz                                   */
/*     Position: 0,0                                                  */
/*     Transform: normal                                              */
/*     Scale: 1.000000                                                */
/*     Adaptive Sync: disabled                                        */
/* ------------------------------------------------------------------ */

static int parse_output_line(const char *line, struct wlr_output_info *out)
{
    const char *q1, *q2;
    size_t n;

    if (line[0] == '\0' || isspace((unsigned char)line[0]))
        return 0;

    memset(out, 0, sizeof(*out));
    out->enabled = 1;

    q1 = strchr(line, '"');
    if (q1)
    {
        n = (size_t)(q1 - line);
        while (n > 0 && isspace((unsigned char)line[n - 1])) n--;
        if (n >= sizeof(out->name)) n = sizeof(out->name) - 1;
        memcpy(out->name, line, n);
        out->name[n] = '\0';

        q2 = strchr(q1 + 1, '"');
        if (q2)
        {
            n = (size_t)(q2 - q1 - 1);
            if (n >= sizeof(out->description)) n = sizeof(out->description) - 1;
            memcpy(out->description, q1 + 1, n);
            out->description[n] = '\0';
        }
    }
    else
    {
        n = strlen(line);
        if (n >= sizeof(out->name)) n = sizeof(out->name) - 1;
        memcpy(out->name, line, n);
        out->name[n] = '\0';
    }
    return 1;
}

static int parse_mode_line(const char *p, struct wlr_mode_info *mode)
{
    unsigned int w, h;
    float hz;

    if (sscanf(p, "%ux%u px, %f Hz", &w, &h, &hz) != 3)
        return 0;

    memset(mode, 0, sizeof(*mode));
    mode->width       = w;
    mode->height      = h;
    mode->refresh_mhz = (unsigned int)(hz * 1000.0f + 0.5f);
    mode->preferred   = (strstr(p, "preferred") != NULL);
    mode->current     = (strstr(p, "current")   != NULL);
    return 1;
}

/* ------------------------------------------------------------------ */
/* unix_wlr_enumerate                                                 */
/* ------------------------------------------------------------------ */

static NTSTATUS wlr_enumerate(void *args)
{
    struct wlr_enumerate_params *params = args;
    FILE *fp;
    char line[512];
    int cur = -1;
    int in_modes = 0;

    memset(params, 0, sizeof(*params));

    fp = popen("wlr-randr 2>/dev/null", "r");
    if (!fp)
        return STATUS_UNSUCCESSFUL;

    while (fgets(line, sizeof(line), fp))
    {
        const char *p;

        strip_trailing(line);
        if (line[0] == '\0') continue;

        /* --- output header (column 0) ----------------------------- */
        if (!isspace((unsigned char)line[0]))
        {
            if (params->num_outputs >= MAX_WLR_OUTPUTS) break;
            cur = (int)params->num_outputs;
            if (parse_output_line(line, &params->outputs[cur]))
                params->num_outputs++;
            else
                cur = -1;
            in_modes = 0;
            continue;
        }

        if (cur < 0) continue;

        p = line;
        while (isspace((unsigned char)*p)) p++;

        /* --- Enabled ----------------------------------------------- */
        if (strncmp(p, "Enabled:", 8) == 0)
        {
            const char *v = p + 8;
            while (isspace((unsigned char)*v)) v++;
            params->outputs[cur].enabled = (strncmp(v, "yes", 3) == 0);
            in_modes = 0;
            continue;
        }

        /* --- Position ---------------------------------------------- */
        if (strncmp(p, "Position:", 9) == 0)
        {
            sscanf(p + 9, " %d,%d",
                   &params->outputs[cur].pos_x,
                   &params->outputs[cur].pos_y);
            in_modes = 0;
            continue;
        }

        /* --- Transform --------------------------------------------- */
        if (strncmp(p, "Transform:", 10) == 0)
        {
            params->outputs[cur].transform = parse_transform(p + 10);
            in_modes = 0;
            continue;
        }

        /* --- Adaptive Sync ----------------------------------------- */
        if (strncmp(p, "Adaptive Sync:", 14) == 0)
        {
            const char *v = p + 14;
            while (isspace((unsigned char)*v)) v++;
            params->outputs[cur].adaptive_sync = (strncmp(v, "enabled", 7) == 0);
            in_modes = 0;
            continue;
        }

        /* --- Modes: header ----------------------------------------- */
        if (strncmp(p, "Modes:", 6) == 0)
        {
            in_modes = 1;
            continue;
        }

        /* --- other labelled fields end modes block ----------------- */
        if (!in_modes) continue;
        if (strchr(p, ':') && !isdigit((unsigned char)*p))
        {
            in_modes = 0;
            continue;
        }

        /* --- mode line --------------------------------------------- */
        if (params->outputs[cur].num_modes < MAX_WLR_MODES)
        {
            struct wlr_mode_info mode;
            if (parse_mode_line(p, &mode))
                params->outputs[cur].modes[params->outputs[cur].num_modes++] = mode;
        }
    }

    pclose(fp);
    return params->num_outputs ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

/* ------------------------------------------------------------------ */
/* unix_wlr_apply                                                     */
/*                                                                    */
/* Builds a single wlr-randr invocation from the flags + fields.      */
/* ------------------------------------------------------------------ */

static NTSTATUS wlr_apply(void *args)
{
    struct wlr_apply_params *p = args;
    char cmd[1024];
    int  pos;
    int  ret;

    if (!safe_output_name(p->output_name))
        return STATUS_INVALID_PARAMETER;

    pos = snprintf(cmd, sizeof(cmd), "wlr-randr --output %s", p->output_name);

    if (p->flags & WLR_APPLY_MODE)
    {
        if (p->flags & WLR_APPLY_REFRESH)
        {
            float hz = p->refresh_mhz / 1000.0f;
            pos += snprintf(cmd + pos, sizeof(cmd) - pos,
                            " --mode %ux%u@%.6fHz", p->width, p->height, hz);
        }
        else
        {
            pos += snprintf(cmd + pos, sizeof(cmd) - pos,
                            " --mode %ux%u", p->width, p->height);
        }
    }
    else if (p->flags & WLR_APPLY_REFRESH)
    {
        /* Refresh-only change: need to re-state the current resolution.
         * wlr-randr requires --mode for the Hz part. We enumerate to
         * find the current width/height for this output. */
        struct wlr_enumerate_params ep;
        unsigned int i, m;
        unsigned int w = 0, h = 0;
        float hz = p->refresh_mhz / 1000.0f;

        memset(&ep, 0, sizeof(ep));
        {
            FILE *fp = popen("wlr-randr 2>/dev/null", "r");
            if (fp)
            {
                /* quick re-parse just for this output's current mode */
                char line[512];
                int found_output = 0;
                while (fgets(line, sizeof(line), fp))
                {
                    strip_trailing(line);
                    if (!isspace((unsigned char)line[0]))
                    {
                        found_output = (strstr(line, p->output_name) == line);
                        continue;
                    }
                    if (found_output)
                    {
                        const char *lp = line;
                        while (isspace((unsigned char)*lp)) lp++;
                        unsigned int tw, th;
                        float thz;
                        if (sscanf(lp, "%ux%u px, %f Hz", &tw, &th, &thz) == 3 &&
                            strstr(lp, "current"))
                        {
                            w = tw; h = th;
                            break;
                        }
                    }
                }
                pclose(fp);
            }
        }
        if (w == 0)
            return STATUS_UNSUCCESSFUL;

        pos += snprintf(cmd + pos, sizeof(cmd) - pos,
                        " --mode %ux%u@%.6fHz", w, h, hz);
    }

    if (p->flags & WLR_APPLY_TRANSFORM)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos,
                        " --transform %s", transform_str(p->transform));

    if (p->flags & WLR_APPLY_VRR)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos,
                        " --adaptive-sync %s",
                        p->adaptive_sync ? "enabled" : "disabled");

    /* silence stderr */
    snprintf(cmd + pos, sizeof(cmd) - pos, " 2>/dev/null");

    ret = system(cmd);
    if (ret != 0 && (p->flags & WLR_APPLY_REFRESH) && (p->flags & WLR_APPLY_MODE))
    {
        /* retry without exact refresh — let compositor pick closest */
        pos = snprintf(cmd, sizeof(cmd), "wlr-randr --output %s --mode %ux%u",
                       p->output_name, p->width, p->height);
        if (p->flags & WLR_APPLY_TRANSFORM)
            pos += snprintf(cmd + pos, sizeof(cmd) - pos,
                            " --transform %s", transform_str(p->transform));
        if (p->flags & WLR_APPLY_VRR)
            pos += snprintf(cmd + pos, sizeof(cmd) - pos,
                            " --adaptive-sync %s",
                            p->adaptive_sync ? "enabled" : "disabled");
        snprintf(cmd + pos, sizeof(cmd) - pos, " 2>/dev/null");
        ret = system(cmd);
    }

    return (ret == 0) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

/* ------------------------------------------------------------------ */
/* entry-point table                                                  */
/* ------------------------------------------------------------------ */

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    [unix_wlr_enumerate] = wlr_enumerate,
    [unix_wlr_apply]     = wlr_apply,
};

C_ASSERT(ARRAY_SIZE(__wine_unix_call_funcs) == unix_funcs_count);