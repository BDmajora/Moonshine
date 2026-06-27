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
#include <stddef.h>
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
 *           lattice_handshake
 *
 * Send CL_HELLO (carrying reconnect_token, 0 for a fresh session) on `fd` and
 * read CL_WELCOME, recording the negotiated version / screen / token. The
 * welcome parse is size-tolerant so a v1 server (no token) still works.
 */
static BOOL lattice_handshake(int fd, uint32_t reconnect_token)
{
    struct cl_hello hello = {
        .type = CL_HELLO, .magic = CL_MAGIC,
        .version = CL_VERSION, .min_version = CL_MIN_VERSION,
        .reconnect_token = reconnect_token,
    };
    char buf[256];
    int fds[CL_MAX_FDS], nfds = 0;
    struct cl_welcome *w;
    ssize_t n;

    if (cl_send(fd, &hello, sizeof(hello), NULL, 0) != 0)
    {
        WARN("failed to send CL_HELLO: %s\n", strerror(errno));
        return FALSE;
    }

    n = cl_recv(fd, buf, sizeof(buf), fds, &nfds);
    while (nfds-- > 0) close(fds[nfds]); /* welcome carries no fds; be safe */
    if (n < (ssize_t)offsetof(struct cl_welcome, reconnect_token) ||
        ((struct cl_welcome *)buf)->type != CL_WELCOME)
    {
        WARN("CL_WELCOME handshake failed (n=%zd)\n", n);
        return FALSE;
    }

    w = (struct cl_welcome *)buf;
    if (w->magic != CL_MAGIC || w->version < CL_MIN_VERSION)
    {
        WARN("incompatible glacier: magic %#x version %u (need >= %u)\n",
             w->magic, w->version, CL_MIN_VERSION);
        return FALSE;
    }

    process_lattice.version = w->version;
    process_lattice.screen_w = w->screen_w;
    process_lattice.screen_h = w->screen_h;
    if (n >= (ssize_t)sizeof(struct cl_welcome))
        process_lattice.reconnect_token = w->reconnect_token;
    return TRUE;
}

/***********************************************************************
 *           lattice_process_init
 *
 * Connect and complete a fresh handshake.  Returns FALSE if glacier is not
 * reachable, which makes DllMain fail the load so Wine falls back to the next
 * graphics driver in the list.
 */
BOOL lattice_process_init(void)
{
    int fd = lattice_connect();
    if (fd < 0) return FALSE;

    if (!lattice_handshake(fd, 0 /* fresh */))
    {
        close(fd);
        return FALSE;
    }

    process_lattice.fd = fd;
    process_lattice.next_wid = 0;
    process_lattice.initialized = TRUE;

    TRACE("welcome: v%u, virtual screen %dx%d, token %u\n",
          process_lattice.version, process_lattice.screen_w,
          process_lattice.screen_h, process_lattice.reconnect_token);
    return TRUE;
}

/***********************************************************************
 *           lattice_reconnect
 *
 * Re-establish the link after glacier drops/restarts, presenting our token so
 * the server hands back the windows it preserved (Phase 6 crash resilience).
 * Retries for a few seconds; FALSE if glacier stays gone (the caller then lets
 * the process terminate).  Runs on the event thread.
 */
BOOL lattice_reconnect(void)
{
    int fd;

    /* Stop window threads from sending on the dead fd while we reconnect. */
    pthread_mutex_lock(&process_lattice.send_mutex);
    if (process_lattice.fd >= 0) close(process_lattice.fd);
    process_lattice.fd = -1;
    pthread_mutex_unlock(&process_lattice.send_mutex);

    for (int tries = 0; tries < 50; tries++)   /* ~5s of 100ms backoff */
    {
        if ((fd = lattice_connect()) >= 0 &&
            lattice_handshake(fd, process_lattice.reconnect_token))
        {
            pthread_mutex_lock(&process_lattice.send_mutex);
            process_lattice.fd = fd;
            pthread_mutex_unlock(&process_lattice.send_mutex);
            TRACE("reconnected to glacier (fd %d, token %u)\n",
                  fd, process_lattice.reconnect_token);
            return TRUE;
        }
        if (fd >= 0) close(fd);
        usleep(100000);
    }

    ERR("could not reconnect to glacier; giving up\n");
    return FALSE;
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
