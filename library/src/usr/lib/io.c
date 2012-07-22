/*
** russ_io.c
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
* Guaranteed write with timeout.
*
* All bytes are written unless a timeout or unrecoverable error
* happens.
*
* @param fd		descriptor
* @param b		buffer
* @param count		# of bytes to write
* @param timeout	time in which to complete call
* @return		# of bytes written; < count on error
*/
ssize_t
russ_writen_timeout(int fd, char *b, size_t count, russ_timeout timeout) {
	struct pollfd	poll_fds[1];
	int		rv, due_time;
	ssize_t		n;
	char		*bend;

	poll_fds[0].fd = fd;
	poll_fds[0].events = POLLOUT|POLLHUP;

	bend = b+count;
	while (b < bend) {
		rv = russ_poll(poll_fds, 1, timeout);
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
* @param timeout	timeout at this time (time, RUSS_TIMEOUT_NEVER, RUSS_TIMEOUT_NOW)
* @return		value as returned by system poll
*/
int
russ_poll(struct pollfd *poll_fds, int nfds, russ_timeout timeout) {
	russ_timeout	deadline;
	int		poll_timeout;
	int		rv;

	switch (timeout) {
	case RUSS_TIMEOUT_NEVER:
		deadline = -1;
		poll_timeout = -1;
		break;
	case RUSS_TIMEOUT_NOW:
		deadline = 0;
		poll_timeout = 0;
		break;
	default:
		deadline = (time(NULL)*1000)+timeout;
	}

	while ((deadline > time(NULL)) || (deadline == RUSS_TIMEOUT_NEVER) || (deadline == RUSS_TIMEOUT_NOW)) {
//fprintf(stderr, "russ_poll rv (%d) errno (%d)\n", rv, errno);

		if ((deadline != RUSS_TIMEOUT_NEVER) && (deadline != RUSS_TIMEOUT_NOW)) {
			poll_timeout = deadline-(time(NULL)*1000);
			poll_timeout = MAX(0, poll_timeout);
		}
		if ((rv = poll(poll_fds, nfds, poll_timeout)) < 0) {
			if (errno != EINTR) {
				break;
			}
		}
		if ((deadline == RUSS_TIMEOUT_NOW) || (rv > 0)) {
			break;
		}
	}
//fprintf(stderr, "poll_fds (%d) nfds (%d) poll_timeout (%d)\n", poll_fds[0], nfds, poll_timeout);
//fprintf(stderr, "russ_poll rv (%d) errno (%d)\n", rv, errno);
	return rv;
}