/* CrystallineLattice — framed datagram I/O with SCM_RIGHTS fd passing.
 *
 * VENDORED from the CrystallineLattice (glacier) repo at src/protocol.c.  It is
 * functionally identical to the upstream copy; the only changes are this note,
 * the makedep marker (so Wine builds this into the unixlib, not the PE module),
 * and a restyle to Wine's C90 house style (declarations at block tops) to keep
 * the build warning-clean.  The wire-critical struct layout lives in protocol.h,
 * which is kept byte-identical to the server's copy.  See SPEC.md §13.3.
 *
 * The wire is a SOCK_SEQPACKET unix socket, so each cl_send is one datagram
 * delivered whole to one cl_recv — no reassembly. File descriptors ride as
 * SCM_RIGHTS ancillary data; the kernel duplicates them into the peer. */

#if 0
#pragma makedep unix
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE   /* MSG_CMSG_CLOEXEC, MSG_NOSIGNAL */
#endif
#include "protocol.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int cl_send(int fd, const void *msg, size_t len, const int *fds, int nfds)
{
	struct iovec iov;
	struct msghdr mh;
	union {
		char buf[CMSG_SPACE(sizeof(int) * CL_MAX_FDS)];
		struct cmsghdr align;
	} u;
	ssize_t n;

	if (nfds < 0 || nfds > CL_MAX_FDS)
		return -1;

	memset(&u, 0, sizeof(u));
	memset(&mh, 0, sizeof(mh));
	iov.iov_base = (void *)msg;
	iov.iov_len = len;
	mh.msg_iov = &iov;
	mh.msg_iovlen = 1;

	if (nfds > 0) {
		struct cmsghdr *c;
		mh.msg_control = u.buf;
		mh.msg_controllen = CMSG_SPACE(sizeof(int) * nfds);
		c = CMSG_FIRSTHDR(&mh);
		c->cmsg_level = SOL_SOCKET;
		c->cmsg_type = SCM_RIGHTS;
		c->cmsg_len = CMSG_LEN(sizeof(int) * nfds);
		memcpy(CMSG_DATA(c), fds, sizeof(int) * nfds);
	}

	do {
		n = sendmsg(fd, &mh, MSG_NOSIGNAL);
	} while (n < 0 && errno == EINTR);
	return n < 0 ? -1 : 0;
}

ssize_t cl_recv(int fd, void *buf, size_t cap, int *fds, int *nfds)
{
	struct iovec iov;
	struct msghdr mh;
	struct cmsghdr *c;
	union {
		char buf[CMSG_SPACE(sizeof(int) * CL_MAX_FDS)];
		struct cmsghdr align;
	} u;
	ssize_t n;
	int got = 0;
	int i;

	if (nfds)
		*nfds = 0;

	memset(&u, 0, sizeof(u));
	memset(&mh, 0, sizeof(mh));
	iov.iov_base = buf;
	iov.iov_len = cap;
	mh.msg_iov = &iov;
	mh.msg_iovlen = 1;
	mh.msg_control = u.buf;
	mh.msg_controllen = sizeof(u.buf);

	do {
		n = recvmsg(fd, &mh, MSG_CMSG_CLOEXEC);
	} while (n < 0 && errno == EINTR);
	if (n <= 0)
		return n;

	for (c = CMSG_FIRSTHDR(&mh); c; c = CMSG_NXTHDR(&mh, c)) {
		if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) {
			got = (int)((c->cmsg_len - CMSG_LEN(0)) / sizeof(int));
			if (got > CL_MAX_FDS)
				got = CL_MAX_FDS;
			if (fds)
				memcpy(fds, CMSG_DATA(c), sizeof(int) * got);
		}
	}
	/* A truncated control buffer would silently leak fds — guard against it. */
	if (mh.msg_flags & MSG_CTRUNC) {
		for (i = 0; i < got && fds; i++)
			close(fds[i]);
		got = 0;
	}
	if (nfds)
		*nfds = got;
	return n;
}

void cl_default_socket_path(char *buf, size_t len)
{
	const char *override = getenv(CL_SOCKET_ENV);
	const char *rt;

	if (override && override[0]) {
		snprintf(buf, len, "%s", override);
		return;
	}
	rt = getenv("XDG_RUNTIME_DIR");
	if (rt && rt[0])
		snprintf(buf, len, "%s/%s", rt, CL_SOCKET_NAME);
	else
		snprintf(buf, len, "/tmp/%s", CL_SOCKET_NAME);
}
