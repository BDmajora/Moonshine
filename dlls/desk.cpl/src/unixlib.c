/*
 * desk.cpl — Unix-side library.
 *
 * Talks to the glacier display server over its control socket
 * ($GLACIER_CTL_SOCKET, else $XDG_RUNTIME_DIR/glacier-ctl) to enumerate outputs
 * and modes and to apply resolution changes — the server is the modesetting
 * authority and does the atomic commit itself. This replaces the old
 * wlr-randr/popen path (which only worked under the wlroots-based frostedglass
 * compositor); see CrystallineLattice DESIGN.md §1/§3.4.
 *
 * The control wire format is duplicated from glacier include/control.h and must
 * stay byte-compatible with it (kept tiny on purpose).
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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "wine/unixlib.h"

#include "../include/unixlib.h"

/* ---- glacier control wire format (mirror of glacier include/control.h) ---- */

#define GLCTL_MAGIC        0x4743544Cu  /* 'GCTL' */
#define GLCTL_SOCKET_ENV   "GLACIER_CTL_SOCKET"
#define GLCTL_SOCKET_NAME  "glacier-ctl"
#define GLCTL_MAX_OUTPUTS  4
#define GLCTL_MAX_MODES    64

enum { GLCTL_GET_OUTPUTS = 1, GLCTL_SET_MODE, GLCTL_OUTPUTS = 128, GLCTL_RESULT };
#define GLCTL_MODE_PREFERRED 0x1u
#define GLCTL_MODE_CURRENT   0x2u

struct glctl_mode { uint32_t w, h, refresh_mhz, flags; };
struct glctl_output {
    char     name[32];
    int32_t  x, y;
    uint32_t num_modes;
    struct glctl_mode modes[GLCTL_MAX_MODES];
};
struct glctl_request {
    uint32_t type, magic;
    char     output[32];
    uint32_t w, h, refresh_mhz;
};
struct glctl_outputs {
    uint32_t type, magic, num_outputs;
    struct glctl_output outputs[GLCTL_MAX_OUTPUTS];
};
struct glctl_result { uint32_t type, magic, ok; };

/* ------------------------------------------------------------------ */

static int connect_control(void)
{
    const char *env = getenv(GLCTL_SOCKET_ENV);
    const char *rt;
    char path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    int fd;

    if (env && env[0])
        snprintf(path, sizeof(path), "%s", env);
    else {
        rt = getenv("XDG_RUNTIME_DIR");
        snprintf(path, sizeof(path), "%s/%s",
                 (rt && rt[0]) ? rt : "/tmp", GLCTL_SOCKET_NAME);
    }

    fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* ---- enumerate outputs/modes from glacier ------------------------- */

static NTSTATUS wlr_enumerate(void *args)
{
    struct wlr_enumerate_params *params = args;
    struct glctl_request req = { .type = GLCTL_GET_OUTPUTS, .magic = GLCTL_MAGIC };
    struct glctl_outputs out;
    ssize_t n;
    unsigned int no;
    int fd;

    memset(params, 0, sizeof(*params));

    if ((fd = connect_control()) < 0)
        return STATUS_UNSUCCESSFUL;
    if (send(fd, &req, sizeof(req), MSG_NOSIGNAL) < 0) {
        close(fd);
        return STATUS_UNSUCCESSFUL;
    }
    n = recv(fd, &out, sizeof(out), 0);
    close(fd);
    if (n < (ssize_t)sizeof(out) || out.type != GLCTL_OUTPUTS ||
        out.magic != GLCTL_MAGIC)
        return STATUS_UNSUCCESSFUL;

    no = out.num_outputs;
    if (no > MAX_WLR_OUTPUTS) no = MAX_WLR_OUTPUTS;
    for (unsigned int i = 0; i < no; i++) {
        struct glctl_output *go = &out.outputs[i];
        struct wlr_output_info *wo = &params->outputs[params->num_outputs++];
        unsigned int nm = go->num_modes;

        memset(wo, 0, sizeof(*wo));
        snprintf(wo->name, sizeof(wo->name), "%s", go->name);
        snprintf(wo->description, sizeof(wo->description), "%s", go->name);
        wo->enabled = 1;
        wo->pos_x = go->x;
        wo->pos_y = go->y;
        wo->transform = WLR_TRANSFORM_NORMAL;

        if (nm > MAX_WLR_MODES) nm = MAX_WLR_MODES;
        for (unsigned int j = 0; j < nm; j++) {
            struct glctl_mode *gm = &go->modes[j];
            struct wlr_mode_info *m = &wo->modes[wo->num_modes++];
            m->width       = gm->w;
            m->height      = gm->h;
            m->refresh_mhz = gm->refresh_mhz;
            m->preferred   = (gm->flags & GLCTL_MODE_PREFERRED) ? 1 : 0;
            m->current     = (gm->flags & GLCTL_MODE_CURRENT) ? 1 : 0;
        }
    }

    return params->num_outputs ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

/* ---- apply a resolution change ------------------------------------ */

static NTSTATUS wlr_apply(void *args)
{
    struct wlr_apply_params *p = args;
    struct glctl_request req = { .type = GLCTL_SET_MODE, .magic = GLCTL_MAGIC };
    struct glctl_result r;
    ssize_t n;
    int fd;

    /* glacier currently honours resolution (and refresh); transform / VRR /
     * layout-position aren't modelled yet, so a change with no new mode is a
     * successful no-op rather than an error. */
    if (!(p->flags & WLR_APPLY_MODE))
        return STATUS_SUCCESS;

    req.w = p->width;
    req.h = p->height;
    if (p->flags & WLR_APPLY_REFRESH)
        req.refresh_mhz = p->refresh_mhz;
    snprintf(req.output, sizeof(req.output), "%.*s",
             (int)sizeof(req.output) - 1, p->output_name);

    if ((fd = connect_control()) < 0)
        return STATUS_UNSUCCESSFUL;
    if (send(fd, &req, sizeof(req), MSG_NOSIGNAL) < 0) {
        close(fd);
        return STATUS_UNSUCCESSFUL;
    }
    n = recv(fd, &r, sizeof(r), 0);
    close(fd);
    if (n < (ssize_t)sizeof(r) || r.type != GLCTL_RESULT || r.magic != GLCTL_MAGIC)
        return STATUS_UNSUCCESSFUL;

    return r.ok ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

/* ------------------------------------------------------------------ */

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    [unix_wlr_enumerate] = wlr_enumerate,
    [unix_wlr_apply]     = wlr_apply,
};

C_ASSERT(ARRAY_SIZE(__wine_unix_call_funcs) == unix_funcs_count);
