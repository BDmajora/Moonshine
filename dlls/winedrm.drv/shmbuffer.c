/*
 * winedrm.drv — shared-memory content buffers
 *
 * Each buffer is an anonymous memfd of XRGB8888 pixels: mmap'd so GDI can paint
 * into it, and handed to glacier by file descriptor (SCM_RIGHTS) on every
 * CL_COMMIT.  The same fd is reused across commits — glacier dup()s it.
 * dma-buf/GBM allocation (CL_BUF_DMABUF) is the M4 upgrade (SPEC §8.4).
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

#include "config.h" /* defines _GNU_SOURCE, needed for memfd_create */

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "lattice.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedrm);

struct drm_shm_buffer *drm_shm_buffer_create(int width, int height)
{
    struct drm_shm_buffer *buffer;
    int stride = width * WINEDRM_BYTES_PER_PIXEL;
    size_t size = (size_t)stride * height;

    if (width <= 0 || height <= 0 || size == 0)
    {
        ERR("invalid buffer size %dx%d\n", width, height);
        return NULL;
    }

    if (!(buffer = calloc(1, sizeof(*buffer)))) return NULL;

    buffer->fd = memfd_create("winedrm-buffer", MFD_CLOEXEC);
    if (buffer->fd < 0)
    {
        ERR("memfd_create failed\n");
        goto err;
    }
    if (ftruncate(buffer->fd, size) != 0)
    {
        ERR("ftruncate(%zu) failed\n", size);
        goto err;
    }

    buffer->map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, buffer->fd, 0);
    if (buffer->map == MAP_FAILED)
    {
        buffer->map = NULL;
        ERR("mmap(%zu) failed\n", size);
        goto err;
    }

    buffer->ref = 1;
    buffer->width = width;
    buffer->height = height;
    buffer->stride = stride;
    buffer->map_size = size;

    TRACE("%p %dx%d stride=%d fd=%d map=%p\n", buffer, width, height, stride,
          buffer->fd, buffer->map);
    return buffer;

err:
    if (buffer->fd >= 0) close(buffer->fd);
    free(buffer);
    return NULL;
}

void drm_shm_buffer_ref(struct drm_shm_buffer *buffer)
{
    InterlockedIncrement(&buffer->ref);
}

void drm_shm_buffer_unref(struct drm_shm_buffer *buffer)
{
    if (InterlockedDecrement(&buffer->ref) > 0) return;

    TRACE("destroying %p fd=%d\n", buffer, buffer->fd);
    if (buffer->map) munmap(buffer->map, buffer->map_size);
    if (buffer->fd >= 0) close(buffer->fd);
    free(buffer);
}
