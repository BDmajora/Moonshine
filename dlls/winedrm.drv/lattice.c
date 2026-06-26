/*
 * winedrm.drv — CrystallineLattice connection management
 *
 * Connects to glacier, runs the v0 handshake (CL_HELLO / CL_WELCOME), and
 * serializes outgoing datagrams.  Reference: glacier's own src/client.c.
 *
 * Copyright 2026 the CrystallineLattice / YetiOS project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "lattice.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedrm);

struct lattice process_lattice = { .fd = -1, .send_mutex = PTHREAD_MUTEX_INITIALIZER };

/***********************************************************************
 *           lattice_connect
 *
 * Open a SOCK_SEQPACKET socket and connect to glacier's rendezvous path.
 */
static int lattice_connect(void)
{
    char path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    int fd;

    cl_default_socket_path(path, sizeof(path));

    fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        ERR("failed to create socket: %s\n", strerror(errno));
        return -1;
    }

    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        WARN("could not connect to glacier at %s: %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }

    TRACE("connected to glacier at %s (fd %d)\n", path, fd);
    return fd;
}

/***********************************************************************
 *           lattice_process_init
 *
 * Connect and complete the v0 handshake.  Returns FALSE if glacier is not
 * reachable, which makes DllMain fail the load so Wine falls back to the next
 * graphics driver in the list.
 */
BOOL lattice_process_init(void)
{
    struct cl_hello hello = {
        .type = CL_HELLO, .magic = CL_MAGIC,
        .version = CL_VERSION, .min_version = CL_MIN_VERSION,
    };
    char buf[256];
    int fds[CL_MAX_FDS], nfds = 0;
    struct cl_welcome *welcome;
    ssize_t n;
    int fd;

    if ((fd = lattice_connect()) < 0) return FALSE;

    if (cl_send(fd, &hello, sizeof(hello), NULL, 0) != 0)
    {
        ERR("failed to send CL_HELLO: %s\n", strerror(errno));
        close(fd);
        return FALSE;
    }

    n = cl_recv(fd, buf, sizeof(buf), fds, &nfds);
    while (nfds-- > 0) close(fds[nfds]); /* welcome carries no fds; be safe */
    if (n < (ssize_t)sizeof(struct cl_welcome) ||
        ((struct cl_welcome *)buf)->type != CL_WELCOME)
    {
        ERR("CL_WELCOME handshake failed (n=%zd)\n", n);
        close(fd);
        return FALSE;
    }

    welcome = (struct cl_welcome *)buf;
    if (welcome->magic != CL_MAGIC || welcome->version < CL_MIN_VERSION)
    {
        ERR("incompatible glacier: magic %#x version %u (need >= %u)\n",
            welcome->magic, welcome->version, CL_MIN_VERSION);
        close(fd);
        return FALSE;
    }

    process_lattice.fd = fd;
    process_lattice.version = welcome->version;
    process_lattice.screen_w = welcome->screen_w;
    process_lattice.screen_h = welcome->screen_h;
    process_lattice.next_wid = 0;
    process_lattice.initialized = TRUE;

    TRACE("welcome: v%u, virtual screen %dx%d\n", welcome->version,
          process_lattice.screen_w, process_lattice.screen_h);
    return TRUE;
}

/***********************************************************************
 *           lattice_send
 *
 * Send one datagram to glacier, serialized against concurrent senders (the
 * event thread and any window thread can both reach here).
 */
int lattice_send(const void *msg, size_t len, const int *fds, int nfds)
{
    int ret;

    if (process_lattice.fd < 0) return -1;

    pthread_mutex_lock(&process_lattice.send_mutex);
    ret = cl_send(process_lattice.fd, msg, len, fds, nfds);
    pthread_mutex_unlock(&process_lattice.send_mutex);

    if (ret) WARN("cl_send failed: %s\n", strerror(errno));
    return ret;
}

/***********************************************************************
 *           lattice_alloc_wid
 *
 * Allocate a fresh, nonzero client-scoped window id.
 */
uint32_t lattice_alloc_wid(void)
{
    LONG wid;
    do { wid = InterlockedIncrement(&process_lattice.next_wid); } while (!wid);
    return (uint32_t)wid;
}
