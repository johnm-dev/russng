/*
* lib/io.c
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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "russ_priv.h"

/**
* Close fd with auto retry on EINTR.
*
* @param fd		descriptor
* @return		0 on success; -1 or error
*/
int
russ_close(int fd) {
	while (close(fd) < 0) {
		if (errno != EINTR) {
			return -1;
		}
	}
	return 0;
}

/**
* Read bytes with auto retry on EINTR and EAGAIN.
*
* @param fd		descriptor
* @param[out] b		buffer
* @param count		# of bytes
* @return		# of bytes read or error/EOF
*/
ssize_t
russ_read(int fd, void *b, size_t count) {
	ssize_t	n;

	while ((n = read(fd, b, count)) < 0) {
		if ((errno != EAGAIN) && (errno != EINTR)) {
			/* unrecoverable error */
			break;
		}
	}
	/* bytes read or error */
	return n;
}

/**
* Read a line of bytes, return on success or unrecoverable error
*
* @param fd		descriptor
* @param[out] b		buffer
* @param count		# of bytes
* @return		# of bytes read or error/EOF
*/
ssize_t
russ_readline(int fd, void *b, size_t count) {
	ssize_t	n;
	size_t	total;
	char	ch;

	total = 0;
	while (total < count) {
		if ((n = russ_read(fd, b, 1)) <= 0) {
			break;
		}
		ch = *((char *)b);
		b += n;
		total += n;
		if (ch == '\n') {
			break;
		}
	}
	if (total > 0) {
		/* if error, will show up on next read */
		return total;
	} else {
		/* error */
		return n;
	}
}

/**
* Guaranteed read. Return on success or unrecoverable error/EOF.
*
* @param fd		descriptor
* @param[out] b		buffer
* @param count		# of bytes to read
* @return		# of bytes read; < count on error/EOF
*/
ssize_t
russ_readn(int fd, void *b, size_t count) {
	ssize_t	n;
	void	*bend;

	bend = b+count;
	while (b < bend) {
		if ((n = russ_read(fd, b, bend-b)) <= 0) {
			break;
		}
		b += n;
	}
	return count-(bend-b);
}

/**
* Guaranteed read. Return on success or unrecoverable error/EOF.
*
* @param deadline	deadline to complete operation
* @param fd		descriptor
* @param[out] b		buffer
* @param count		# of bytes to read
* @return		# of bytes read; < count on error/EOF
*/
ssize_t
russ_readn_deadline(russ_deadline deadline, int fd, void *b, size_t count) {
	struct pollfd	pollfds[1];
	int		rv, due_time;
	ssize_t		n;
	void		*bend;

	/* catch fd<0 before calling into poll() */
	if (fd < 0) {
		return -1;
	}

	pollfds[0].fd = fd;
	pollfds[0].events = POLLIN|POLLHUP;

	bend = b+count;
	while (b < bend) {
		if ((rv = russ_poll_deadline(deadline, pollfds, 1)) <= 0) {
			/* error or timeout */
			break;
		} else if (pollfds[0].revents & POLLIN) {
			if ((n = russ_read(fd, b, bend-b)) <= 0) {
				break;
			}
			b += n;
		} else if (pollfds[0].revents & POLLHUP) {
			break;
		}
	}
	return count-(bend-b);
}

/**
* Write bytes with auto retry on EINTR and EAGAIN.
*
* @param fd		descriptor
* @param b		buffer
* @param count		# of bytes to write
* @return		# of bytes written; -1 on error
*/
ssize_t
russ_write(int fd, void *b, size_t count) {
	ssize_t	n;

	while ((n = write(fd, b, count)) < 0) {
		if ((errno != EAGAIN) && (errno != EINTR)) {
			/* unrecoverable error */
			break;
		}
	}
	/* bytes written or error */
	return n;
}

/**
* Guaranteed write.
*
* All bytes are written unless an unrecoverable error happens.
*
* @param fd		descriptor
* @param b		buffer
* @param count		# of bytes to write
* @return		# of bytes written; < count on error
*/
ssize_t
russ_writen(int fd, void *b, size_t count) {
	ssize_t	n;
	void	*bend;

	bend = b+count;
	while (b < bend) {
		if ((n = russ_write(fd, b, bend-b)) < 0) {
			break;
		}
		b += n;
	}
	return count-(bend-b);
}

/**
* Guaranteed write with deadline.
*
* All bytes are written unless the deadline is reached or
* unrecoverable error happens.
*
* @param deadline	deadline to complete call
* @param fd		descriptor
* @param b		buffer
* @param count		# of bytes to write
* @return		# of bytes written; < count on error
*/
ssize_t
russ_writen_deadline(russ_deadline deadline, int fd, void *b, size_t count) {
	struct pollfd	pollfds[1];
	int		rv, due_time;
	ssize_t		n;
	void		*bend;

	/* catch fd<0 before calling into poll() */
	if (fd < 0) {
		return -1;
	}

	pollfds[0].fd = fd;
	pollfds[0].events = POLLOUT|POLLHUP;

	bend = b+count;
	while (b < bend) {
		if ((rv = russ_poll_deadline(deadline, pollfds, 1)) <= 0) {
			/* error or timeout */
			break;
		} else if (pollfds[0].revents & POLLOUT) {
			if ((n = write(fd, b, bend-b)) < 0) {
				break;
			}
			b += n;
		} else if (pollfds[0].revents & POLLHUP) {
			break;
		}

	}
	return count-(bend-b);
}

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

#if 0
	/* catch fd<0 before calling into poll() */
	if (sd < 0) {
		return -1;
	}
#endif

	pollfds[0].fd = sd;
	pollfds[0].events = POLLIN;
	while (1) {
#if 0
		if ((rv = poll(pollfds, 1, russ_to_timeout(deadline))) > 0) {
			return accept(sd, addr, addrlen);
		} else if (rv == 0) {
			errno = 0; /* reset */
			return -1;
		} else if (errno != EINTR) {
			return -1;
		}
#endif
		if (((rv = accept(sd, addr, addrlen)) >= 0)
			|| (errno != EINTR)) {
			return rv;
		}
	}
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
		return -1;
	}

	/* save and set non-blocking */
	if (((flags = fcntl(sd, F_GETFL)) < 0)
		|| (fcntl(sd, F_SETFL, flags|O_NONBLOCK) < 0)) {
		return -1;
	}
	if (connect(sd, addr, addrlen) < 0) {
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
		return -1;
	}
	strcpy(servaddr.sun_path, path);

retry:
	if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		return -1;
	}

	/* set to non-blocking */
	if (((flags = fcntl(sd, F_GETFL)) < 0)
		|| (fcntl(sd, F_SETFL, flags|O_NONBLOCK) < 0)) {
		goto cleanup;
	}

	if (connect(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
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
			goto cleanup;
		}
	}

	/* restore blocking */
	if (fcntl(sd, F_SETFL, flags) < 0) {
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
* poll() with automatic restart on EINTR.
*
* @param deadline	deadline to complete operation
* @param pollfds	array of pollfd
* @param nfds		# of descriptors in pollfds
* @return		value as returned by system poll
*/
int
russ_poll_deadline(russ_deadline deadline, struct pollfd *pollfds, int nfds) {
	int	rv;

	while (1) {
//fprintf(stderr, "russ_poll rv (%d) errno (%d)\n", rv, errno);
		if (((rv = poll(pollfds, nfds, russ_to_timeout(deadline))) >= 0)
			|| (errno != EINTR)) {
			/* data (>0), timeout (0), non-EINTR error */
			break;
		}
	}
//fprintf(stderr, "pollfds (%d) nfds (%d) poll_timeout (%d)\n", pollfds[0], nfds, poll_timeout);
//fprintf(stderr, "russ_poll rv (%d) errno (%d)\n", rv, errno);
	return rv;
}
