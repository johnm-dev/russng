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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "russ_priv.h"

/**
* Read bytes with auto retry on EINTR and EAGAIN.
*
* @param fd	input descriptor
* @param b	buffer
* @param count	# of bytes
* @return	# of bytes read or error/EOF
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
* @param fd	input descriptor
* @param b	buffer
* @param count	# of bytes
* @return	# of bytes read or error/EOF
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
* @param fd	input descriptor
* @param b	buffer
* @param count	# of bytes to read
* @return	# of bytes read; < count on error/EOF
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
* @param fd	output descriptor
* @param b	buffer
* @param count	# of bytes to write
* @return	# of bytes written; or error/EOF
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
* Guaranteed write. Return on success or unrecoverable error.
*
* @param fd	output descriptor
* @param b	buffer
* @param count	# of bytes to write
* @return	# of bytes written; < count on error
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
* Guaranteed write. Return on success, timeout, or unrecoverable error.
*
* @param fd		output descriptor
* @param b		buffer
* @param count		# of bytes to write
* @param timeout	time in which to complete call
* @return		# of bytes written; < count on error
*/
ssize_t
russ_writen_timeout(int fd, char *b, size_t count, int timeout) {
	struct pollfd	poll_fds[1];
	int		rv, due_time;
	ssize_t		n;
	char		*bend;

	poll_fds[0].fd = fd;
	poll_fds[0].events = POLLOUT|POLLHUP;
	if (timeout > 0) {
		due_time = time(NULL)+timeout;
	} else {
		due_time = timeout;
	}

	bend = b+count;
	while (b < bend) {
		rv = russ_poll(poll_fds, 1, due_time);
		if ((rv <= 0) || (poll_fds[0].revents & POLLHUP)) {
			break;
		}
		if ((n = russ_write(fd, b, bend-b)) < 0) {
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
* @param due_time	timeout at this time (-1 is infinity, 0 is now)
* @return		value as returned by system poll
*/
int
russ_poll(struct pollfd *poll_fds, int nfds, int due_time) {
	int	poll_timeout;
	int	rv;

	while ((due_time > time(NULL)) || (due_time == -1)) {
fprintf(stderr, "russ_poll rv (%d)\n", rv);
		if ((due_time == -1) || (due_time == 0)) {
			poll_timeout = due_time;
		} else {
			poll_timeout = due_time-time(NULL);
			poll_timeout = MAX(0, poll_timeout)*1000;
		}
		if ((rv = poll(poll_fds, nfds, poll_timeout)) < 0) {
			if (errno != EINTR) {
				break;
			}
		}
		if ((due_time == 0) || (rv > 0)) {
			break;
		}
	}
fprintf(stderr, "russ_poll rv (%d)\n", rv);
	return rv;
}

/**
* Generic function to stream data between descriptors.
*
* @param in_fd	input descriptor
* @param out_fd	output descriptor
* @param count	# of bytes to stream; -1 to stream without limit
* @param blocksize	upto # of bytes to send at a time; also the
*			buffer size
* @return	0 on success, -1 or error/EOF
*/
int
russ_stream_fd(int in_fd, int out_fd, long count, long blocksize) {
	char	*buf;
	long	rv, total;

	if ((buf = malloc(blocksize)) == NULL) {
		return -1;
	}

	if (count == -1) {
		while (1) {
			if ((rv = _stream_fd(in_fd, out_fd, buf, blocksize)) <= 0) {
				return rv;
			}
		}
	} else {
		total = 0;
		while (count > 0) {
			nread = (blocksize < count) ? blocksize : count;
			if ((rv = _steam_fd(in_fd, out_fd, buf, nread)) <= 0) {
				return rv;
			}
			count -= nread;
			total += nread;
		}
	}
	return 0;
}

static
int _stream_fd(int in_fd, int out_fd, char *buf, int bsize) {
	long	nread, nwrite;

	if ((nread = russ_read(in_fd, buf, bsize)) < 0) {
		return -1;
	} else if (nread == 0) {
		return 0;
	}
	if ((nwrite = russ_writen(out_fd, buf, nread)) != nread) {
		return -1;
	}
	return nwrite;
}
