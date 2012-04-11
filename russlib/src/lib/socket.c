/*
** lib/socket.c
*/

/*
# license--start
#
# This file is part of the RUSS library.
# Copyright (C) 2012 John Marshall
#
# The RUSS library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# license--end
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include <fcntl.h>
#endif

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "russ_priv.h"

/**
* Get credentials from socket file. Supports AIX and LINUX.
*
* @param sd	socket descriptor
* @param cred	credentials object in which to put infomation
* @return	0 on success, -1 on error
*/
int
russ_get_credentials(int sd, struct russ_credentials *cred) {
	socklen_t		_cred_len;

#ifdef AIX
	struct peercred_struct	_cred;

	_cred_len = sizeof(struct peercred_struct);
	if (getsockopt(sd, SOL_SOCKET, SO_PEERID, &_cred, &_cred_len) < 0) {
		/*printf("errno (%d)\n", errno);*/
		return -1;
	}
	cred->pid = (long)_cred.pid;
	cred->uid = (long)_cred.euid;
	cred->gid = (long)_cred.egid;

#elif LINUX
	struct ucred	_cred;

	_cred_len = sizeof(struct ucred);
	if (getsockopt(sd, SOL_SOCKET, SO_PEERCRED, &_cred, &_cred_len) < 0) {
		return -1;
	}
	cred->pid = (long)_cred.pid;
	cred->uid = (long)_cred.uid;
	cred->gid = (long)_cred.gid;
#elif FREEBSD
	struct ucred	_cred;

	_cred_len = sizeof(struct ucred);
	if (getsockopt(sd, SOL_SOCKET, SO_PEERCRED, &_cred, &_cred_len) < 0) {
		return -1;
	}
	cred->pid = (long)_cred.pid;
	cred->uid = (long)_cred.uid;
	cred->gid = (long)_cred.gid;
#endif
	return 0;
}

/**
* Receive descriptor over socket (no message support).
*
* @param sd	socket descriptor
* @param fd	integer pointer for received descriptor
* @return	0 on success, -1 on error
*/
#define CMSG_SIZE	CMSG_SPACE(sizeof(int))
int
russ_recvfd(int sd, int *fd) {
	struct msghdr	msgh;
	struct iovec	iov[1];
	struct cmsghdr	*cmsgh;
	char		buf[1], cbuf[CMSG_SIZE];
	int		rv;

	iov[0].iov_base = buf;
	iov[0].iov_len = 1;

	/* message; receive the single byte */
	msgh.msg_name = NULL;
	msgh.msg_namelen = 0;
	msgh.msg_iov = iov;
	msgh.msg_iovlen = 1;

	/* control */
	msgh.msg_control = cbuf;
	msgh.msg_controllen = CMSG_SIZE;
	msgh.msg_flags = 0;

	if ((rv = recvmsg(sd, &msgh, 0)) < 0) {
		return -1;
	}

	cmsgh = CMSG_FIRSTHDR(&msgh);
	if ((cmsgh == NULL)
		|| (cmsgh->cmsg_len != CMSG_LEN(sizeof(int)))
		|| (cmsgh->cmsg_level != SOL_SOCKET)
		|| (cmsgh->cmsg_type != SCM_RIGHTS)) {
		return -1;
	}
	/* TODO: can an fd == -1 ever be sent? */
	*fd = ((int *)CMSG_DATA(cmsgh))[0];
	return 0;
}

/**
* Send descriptor (no message support) over socket.
*
* @param sd	socket descriptor
* @param fd	descriptor to send
* @return	0 on success, -1 on error
*/
int
russ_sendfd(int sd, int fd) {
	struct msghdr	msgh;
	struct iovec	iov[1];
	struct cmsghdr	*cmsgh;
	char		cbuf[CMSG_SIZE];
	int		rv;

	/* initialize */
	memset(&iov[0], 0, sizeof(struct iovec));
	memset(&msgh, 0, sizeof(struct msghdr));
	memset(cbuf, 0, CMSG_SIZE);

	/* message; must sent at least 1 byte */
	msgh.msg_name = NULL;
	msgh.msg_namelen = 0;
	msgh.msg_iov = iov;
	msgh.msg_iovlen = 1;
	iov[0].iov_base = (void *)" ";
	iov[0].iov_len = 1;

	/* control */
	msgh.msg_control = cbuf;
	msgh.msg_controllen = CMSG_SIZE;
	msgh.msg_flags = 0;

	cmsgh = CMSG_FIRSTHDR(&msgh);
	cmsgh->cmsg_len = CMSG_LEN(sizeof(int));
	cmsgh->cmsg_level = SOL_SOCKET;
	cmsgh->cmsg_type = SCM_RIGHTS;
	((int *)CMSG_DATA(cmsgh))[0] = fd;

	return sendmsg(sd, &msgh, 0);
}

#if 0
/*
** forward bytes between in fd and out fd
**
** Linux: optimized using zero-copy splice()
*/
void
russ_forward_bytes(int fd_in, int fd_out) {
#ifdef x_GNU_SOURCE
	int	cnt;

	while (1) {
/*fprintf(stderr, "calling splice\n");*/
		cnt = splice(fd_in, NULL, fd_out, NULL, FORWARD_BLOCK_SIZE, 0);
	}
#else /* _GNU_SOURCE */
	char	buf[FORWARD_BLOCK_SIZE];
	int	cnt, rn, wn;

	while (1) {
		rn = read(fd_in, buf, FORWARD_BLOCK_SIZE);
/*fprintf(stderr, "rn (%d) buf (%20s)\n", rn, buf);*/
		if (rn > 0) {
			for (wn = 0; wn < rn; ) {
				cnt = write(fd_out, &buf[wn], rn-wn);
				wn += cnt;
			}
		} else if (rn < 0) {
			/* error */
			if (errno != EINTR) {
				break;
			}
		} else {
			/* EOF */
			break;
		}
	}
#endif /* _GNU_SOURCE */

/*fprintf(stderr, "exiting russ_forward_bytes\n");*/
	close(fd_in);
	close(fd_out);
}

void *
russ_forward_bytes_vp(void *vp) {
	struct pipe_fds	*pfds;

	pfds = (struct pipe_fds *)vp;
	russ_forward_bytes(pfds->in_fd, pfds->out_fd);
}
#endif
