/*
** russ_io.c
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
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "russ_priv.h"

/**
* Read bytes with auto retry on EINTR and EAGAIN.
*
* @param fd		descriptor
* @param[out] b		buffer
* @param count		# of bytes
* @return		# of bytes read or error/EOF
*/
ssize_t
russ_read(int fd, char *b, size_t count) {
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
russ_readline(int fd, char *b, size_t count) {
	ssize_t	n;
	size_t	total;
	char	ch;

	total = 0;
	while (total < count) {
		if ((n = russ_read(fd, b, 1)) <= 0) {
			break;
		}
		ch = *b;
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
russ_readn(int fd, char *b, size_t count) {
	ssize_t	n;
	char	*bend;

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
* @param fd		descriptor
* @param[out] b		buffer
* @param count		# of bytes to read
* @param deadline	deadline to complete operation
* @return		# of bytes read; < count on error/EOF
*/
ssize_t
russ_readn_deadline(int fd, char *b, size_t count, russ_deadline deadline) {
	struct pollfd	poll_fds[1];
	int		rv, due_time;
	ssize_t		n;
	char		*bend;

	poll_fds[0].fd = fd;
	poll_fds[0].events = POLLIN|POLLHUP;

	bend = b+count;
	while (b < bend) {
		rv = russ_poll(poll_fds, 1, russ_to_timeout(deadline));
		if ((rv <= 0) || (poll_fds[0].revents & POLLHUP)) {
			break;
		}
		if ((n = russ_read(fd, b, bend-b)) <= 0) {
			break;
		}
		b += n;
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
russ_write(int fd, char *b, size_t count) {
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
russ_writen(int fd, char *b, size_t count) {
	ssize_t	n;
	char	*bend;

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
* @param fd		descriptor
* @param b		buffer
* @param count		# of bytes to write
* @param deadline	deadline to complete call
* @return		# of bytes written; < count on error
*/
ssize_t
russ_writen_deadline(int fd, char *b, size_t count, russ_deadline deadline) {
	struct pollfd	poll_fds[1];
	int		rv, due_time;
	ssize_t		n;
	char		*bend;

	poll_fds[0].fd = fd;
	poll_fds[0].events = POLLOUT|POLLHUP;

	bend = b+count;
	while (b < bend) {
		rv = russ_poll(poll_fds, 1, russ_to_timeout(deadline));
		if ((rv <= 0) || (poll_fds[0].revents & POLLHUP)) {
			break;
		}
		if ((n = write(fd, b, bend-b)) < 0) {
			break;
		}
		b += n;
	}
	return count-(bend-b);
}

/**
* Guaranteed modified poll with automatic restart on EINTR.
*
* @param poll_fds	initialized pollfd structure
* @param nfds		# of descriptors in poll_fds
* @param deadline	deadline to complete operation
* @return		value as returned by system poll
*/
int
russ_poll(struct pollfd *poll_fds, int nfds, russ_deadline deadline) {
	int	rv;

	while (1) {
//fprintf(stderr, "russ_poll rv (%d) errno (%d)\n", rv, errno);
		if (((rv = poll(poll_fds, nfds, russ_to_timeout(deadline))) >= 0)
			|| (errno != EINTR)) {
			/* data (>0), timeout (0), non-EINTR error */
			break;
		}
	}
//fprintf(stderr, "poll_fds (%d) nfds (%d) poll_timeout (%d)\n", poll_fds[0], nfds, poll_timeout);
//fprintf(stderr, "russ_poll rv (%d) errno (%d)\n", rv, errno);
	return rv;
}
