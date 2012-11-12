/*
** lib/socket.c
*/

/*
# license--start
#
# Copyright 2012 John Marshall
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# license--end
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include <fcntl.h>
#endif

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "russ_priv.h"

/**
* Get credentials from socket file.
*
* Supports:
* AIX - pid, euid, egid
* LINUX - pid, uid, gid
* FREEBSD - uid, gid; pid is unavailable and set to -1
*
* @param sd		socket descriptor
* @param creds		credentials object in which to put infomation
* @return		0 on success; -1 on error
*/
int
russ_get_creds(int sd, struct russ_creds *creds) {
	socklen_t		_cred_len;

#ifdef AIX
	struct peercred_struct	_cred;

	_cred_len = sizeof(struct peercred_struct);
	if (getsockopt(sd, SOL_SOCKET, SO_PEERID, &_cred, &_cred_len) < 0) {
		/*printf("errno (%d)\n", errno);*/
		return -1;
	}
	creds->pid = (long)_cred.pid;
	creds->uid = (long)_cred.euid;
	creds->gid = (long)_cred.egid;

#elif LINUX
	struct ucred	_cred;

	_cred_len = sizeof(struct ucred);
	if (getsockopt(sd, SOL_SOCKET, SO_PEERCRED, &_cred, &_cred_len) < 0) {
		return -1;
	}
	creds->pid = (long)_cred.pid;
	creds->uid = (long)_cred.uid;
	creds->gid = (long)_cred.gid;
#elif FREEBSD
	if (getpeereid(sd, (uid_t *)&(creds->uid), (gid_t *)&(creds->gid)) < 0) {
		return -1;
	}
	creds->pid = -1;
#elif FREEBSD_ALT
	struct xucred	_cred;

	_cred_len = sizeof(struct xucred);
	if (getsockopt(sd, SOL_SOCKET, LOCAL_PEERCRED, &_cred, &_cred_len) < 0) {
		return -1;
	}
	creds->pid = (long)-1;
	creds->uid = (long)_cred.cr_uid;
	creds->gid = (long)_cred.cr_groups[0];
#endif
	return 0;
}

/**
* Receive descriptor over socket.
*
* Only the descriptor is obtained--no message support.
*
* @param sd		socket descriptor
* @param fd		integer pointer for received descriptor
* @return		0 on success; -1 on error
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
* Send descriptor over socket.
*
* Send descriptor only--no message support.
*
* @param sd		socket descriptor
* @param fd		descriptor to send
* @return		0 on success; -1 on error
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