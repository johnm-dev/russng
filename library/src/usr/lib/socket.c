/*
* lib/socket.c
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
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "russ/priv.h"

/**
* accept() with automatic restart on EINTR.
*
* @param sd		socket descriptor
* @param addr		socket address structure
* @param addrlen[in,out]	socket address structure length
* @param deadline	deadline to complete operation
* @return		value as returned from accept; -1 on failure
*/
int
russ_accept_deadline(russ_deadline deadline, int sd, struct sockaddr *addr, socklen_t *addrlen) {
	struct pollfd	pollfds[1];
	int		rv;

#if 1
	/* catch fd<0 before calling into poll() */
	if (sd < 0) {
		return -1;
	}
#endif

	pollfds[0].fd = sd;
	pollfds[0].events = POLLIN;
#if 1
	if ((rv = russ_poll_deadline(deadline, pollfds, 1)) > 0) {
		return accept(sd, addr, addrlen);
	} else if (rv == 0) {
		/* timeout */
		errno = 0;
		return -1;
	}
	return -1;
#endif
#if 0
	while (1) {
		if (((rv = accept(sd, addr, addrlen)) >= 0)
			|| (errno != EINTR)) {
			return rv;
		}
	}
#endif
}

/**
* Announce service as a socket file.
*
* If the address already exists (EADDRINUSE), then we check to see
* if anything is actually using it. If not, we remove it and try to
* set it up. If the address cannot be "bind"ed, then we exit with
* NULL.
*
* The only way to claim an address that is in use it to forcibly
* remove it from the filesystem first (unlink), then call here.
*
* @param saddr		socket address
* @param mode		file mode of path
* @param uid		owner of path
* @param gid		group owner of path
* @return		listener socket descriptor
*/
int
russ_announce(char *saddr, mode_t mode, uid_t uid, gid_t gid) {
	struct sockaddr_un	servaddr;
	int			lisd;

	if ((saddr == NULL) || ((saddr = russ_spath_resolve(saddr)) == NULL)) {
		return -1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	if (strlen(saddr) >= sizeof(servaddr.sun_path)) {
		return -1;
	}
	strcpy(servaddr.sun_path, saddr);
	if ((lisd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		goto free_saddr;
	}
	if (bind(lisd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		if ((errno == EADDRINUSE)
			&& (connect(lisd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)) {
			/* is something listening? */
			if (errno != ECONNREFUSED) {
				goto close_lisd;
			} else if ((unlink(saddr) < 0)
				|| (bind(lisd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)) {
				goto close_lisd;
			}
		} else {
			goto close_lisd;
		}
	}

	/*
	* (RUSSNG_858) bind affected by umask; chmod not affected by
	* umask; file mode of non-0 indicates socket listen()-ing
	*/
	if ((chmod(saddr, 0) < 0)
		|| (chown(saddr, uid, gid) < 0)
		|| (listen(lisd, RUSS_LISTEN_BACKLOG) < 0)
		|| (chmod(saddr, mode) < 0)) {
		goto close_lisd;
	}
	saddr = russ_free(saddr);
	return lisd;

close_lisd:
	russ_close(lisd);
free_saddr:
	saddr = russ_free(saddr);
	return -1;
}

/**
* connect() with automatic restart on EINTR.
*
* @param deadline	deadline to complete operation
* @param sd		socket descriptor
* @param addr		sockaddr structure
* @param addrlen	sockaddr structure length
* @return		0 on success; -1 on error
*/
int
russ_connect_deadline(russ_deadline deadline, int sd, struct sockaddr *addr, socklen_t addrlen) {
	struct pollfd		pollfds[1];
	int			flags;

	/* catch fd<0 before calling into poll() */
	if (sd < 0) {
		if (RUSS_DEBUG_russ_connect_deadline) {
			fprintf(stderr, "RUSS_DEBUG_russ_dialv:sd < 0\n");
		}
		return -1;
	}

	/* save and set non-blocking */
	if (((flags = fcntl(sd, F_GETFL)) < 0)
		|| (fcntl(sd, F_SETFL, flags|O_NONBLOCK) < 0)) {
		if (RUSS_DEBUG_russ_connect_deadline) {
			fprintf(stderr, "RUSS_DEBUG_russ_connect_deadline:flags < 0 || cannot set O_NONBLOCK\n");
		}
		return -1;
	}
	if (connect(sd, addr, addrlen) < 0) {
		if (RUSS_DEBUG_russ_connect_deadline) {
			fprintf(stderr, "RUSS_DEBUG_russ_connect_deadline:connect() < 0\n");
		}
		if ((errno == EINTR) || (errno == EINPROGRESS)) {
			pollfds[0].fd = sd;
			pollfds[0].events = POLLIN;
			if (russ_poll_deadline(deadline, pollfds, 1) < 0) {
				return -1;
			}
		}
	}
	/* restore */
	if (fcntl(sd, F_SETFL, flags) < 0) {
			if (RUSS_DEBUG_russ_connect_deadline) {
				fprintf(stderr, "RUSS_DEBUG_russ_connect_deadline:fcntl(%d, F_SETFL, %x)\n", sd, flags);
			}
		return -1;
	}
	return 0;
}

/**
* Special connect() for AF_UNIX socket, SOCK_STREAM, with automatic
* restart on EINTR, wait for EINPROGRESS, and retry on EAGAIN.
*
* @param deadline	deadline to complete operation
* @param path		path to socket file
* @return		socket descriptor; -1 on error
*/
int
russ_connectunix_deadline(russ_deadline deadline, char *path) {
	struct sockaddr_un	servaddr;
	socklen_t		addrlen;
	struct pollfd		pollfds[1];
	int			flags, sd;

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	if (strlen(path) >= sizeof(servaddr.sun_path)) {
		if (RUSS_DEBUG_russ_connectunix_deadline) {
			fprintf(stderr, "RUSS_DEBUG_russ_connectunix_deadline:bad path length\n");
		}
		return -1;
	}
	strcpy(servaddr.sun_path, path);

retry:
	if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		if (RUSS_DEBUG_russ_connectunix_deadline) {
			fprintf(stderr, "RUSS_DEBUG_russ_connectunix_deadline:sd < 0\n");
		}
		return -1;
	}

	/* set to non-blocking */
	if (((flags = fcntl(sd, F_GETFL)) < 0)
		|| (fcntl(sd, F_SETFL, flags|O_NONBLOCK) < 0)) {
		if (RUSS_DEBUG_russ_connectunix_deadline) {
			fprintf(stderr, "RUSS_DEBUG_russ_connectunix_deadline:flags < 0 || cannot set O_NONBLOCK\n");
		}
		goto cleanup;
	}

	if (connect(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		if (RUSS_DEBUG_russ_connectunix_deadline) {
			fprintf(stderr, "RUSS_DEBUG_russ_connectunix_deadline:connect() < 0\n");
		}
		if ((errno == EINTR) || (errno == EINPROGRESS) || (errno == EAGAIN)) {
			pollfds[0].fd = sd;
			pollfds[0].events = POLLIN;
			if (russ_poll_deadline(deadline, pollfds, 1) < 0) {
				goto cleanup;
			}
			if (errno == EAGAIN) {
				/* SUSv3: close and retry */
				close(sd);
				goto retry;
			}
		} else {
			if (RUSS_DEBUG_russ_connectunix_deadline) {
				fprintf(stderr, "RUSS_DEBUG_russ_connectunix_deadline:errno = %d\n", errno);
			}
			goto cleanup;
		}
	}

	/* restore blocking */
	if (fcntl(sd, F_SETFL, flags) < 0) {
		if (RUSS_DEBUG_russ_connectunix_deadline) {
			fprintf(stderr, "RUSS_DEBUG_russ_connectunix_deadline:cannot restore blocking\n");
		}
		goto cleanup;
	}
	return sd;

cleanup:
	if (sd >= 0) {
		close(sd);
	}
	return -1;
}

/**
* Get credentials from socket file.
*
* Supports:
* AIX - pid, euid, egid
* LINUX - pid, uid, gid
* FREEBSD, OSX - uid, gid; pid is unavailable and set to -1
*
* @param sd		socket descriptor
* @param creds		credentials object in which to put infomation
* @return		0 on success; -1 on error
*/
int
russ_get_creds(int sd, struct russ_creds *creds) {
	socklen_t		_cred_len;

#ifdef __RUSS_AIX__
	struct peercred_struct	_cred;

	_cred_len = sizeof(struct peercred_struct);
	if (getsockopt(sd, SOL_SOCKET, SO_PEERID, &_cred, &_cred_len) < 0) {
		/*printf("errno (%d)\n", errno);*/
		return -1;
	}
	creds->pid = (long)_cred.pid;
	creds->uid = (long)_cred.euid;
	creds->gid = (long)_cred.egid;

#elif __RUSS_LINUX__
	struct ucred	_cred;

	_cred_len = sizeof(struct ucred);
	if (getsockopt(sd, SOL_SOCKET, SO_PEERCRED, &_cred, &_cred_len) < 0) {
		return -1;
	}
	creds->pid = (long)_cred.pid;
	creds->uid = (long)_cred.uid;
	creds->gid = (long)_cred.gid;
#elif __RUSS_FREEBSD__ || __RUSS_OSX__
	if (getpeereid(sd, (uid_t *)&(creds->uid), (gid_t *)&(creds->gid)) < 0) {
		return -1;
	}
	creds->pid = -1;
#elif __RUSS_FREEBSD_ALT__
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
russ_recv_fd(int sd, int *fd) {
	struct msghdr	msgh;
	struct iovec	iov[1];
	struct cmsghdr	*cmsgh = NULL;
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
russ_send_fd(int sd, int fd) {
	struct msghdr	msgh;
	struct iovec	iov[1];
	struct cmsghdr	*cmsgh = NULL;
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

/**
* Unlink/remove an existing socket file.
*
* Resolves the address and unlinks the file.
*
* @param saddr		socket address
* @return		0 on success; -1 on failure
*/
int
russ_unlink(const char *saddr) {
	if ((saddr = russ_spath_resolve(saddr)) == NULL) {
		return -1;
	}
	if (unlink(saddr) < 0) {
		saddr = russ_free((char *)saddr);
		return -1;
	}
	saddr = russ_free((char *)saddr);
	return 0;
}
