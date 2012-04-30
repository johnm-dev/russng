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
* @param due_time	timeout at this time (-1 is infinity, 0 is now)
* @param poll_fds	initialized pollfd structure
* @param nfds		# of descriptors in poll_fds
* @return		value as returned by system poll
*/
int
russ_poll(russ_timeout due_time, struct pollfd *poll_fds, int nfds) {
	int	poll_timeout;
	int	rv;

	while ((due_time > time(NULL)) || (due_time == -1) || (due_time == 0)) {
//fprintf(stderr, "russ_poll rv (%d) errno (%d)\n", rv, errno);
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
//fprintf(stderr, "poll_fds (%d) nfds (%d) poll_timeout (%d)\n", poll_fds[0], nfds, poll_timeout);
//fprintf(stderr, "russ_poll rv (%d) errno (%d)\n", rv, errno);
	return rv;
}

/*
* Forward a block by bunch of bytes or line.
*
* @param in_fd	input fd
* @param out_fd	output fd
* @param buf	buffer
* @param bsize	buffer size
* @param how	0 for bunch, 1 for line
* @return	# of bytes forwarded, -1 on failure
*/
static int
_forward_block(int in_fd, int out_fd, char *buf, int bsize, int how) {
	long	nread, nwrite;

	if (how == 0) {
		if ((nread = russ_read(in_fd, buf, bsize)) < 0) {
			return -1;
		}
	} else {
		if ((nread = russ_readline(in_fd, buf, bsize)) < 0) {
			return -1;
		}
	}
	if (nread == 0) {
		return 0;
	}
	if ((nwrite = russ_writen(out_fd, buf, nread)) != nread) {
		return -1;
	}
	return nwrite;
}

/*
* The actual byte forwarding code.
*
* @param fwd	forwarding struct
* @return	NULL on success, !NULL on failure
*/
static void *
_forward_bytes(void *_fwd) {
	struct russ_forwarding	*fwd;
	char			buf[1<<20], *bp;
	long			nread, nwrite, count;
	int			rv = 0;

	/* setup */
	fwd = (struct russ_forwarding *)_fwd;
	if (fwd->blocksize <= 1<<20) {
		bp = buf;
	} else {
		if ((bp = malloc(fwd->blocksize)) == NULL) {
			return NULL;
		}
	}

	/* transfer */
	if (fwd->count == -1) {
		while (1) {
			if ((rv = _forward_block(fwd->in_fd, fwd->out_fd, bp, fwd->blocksize, fwd->how)) <= 0) {
				break;
			}
		}
	} else {
		count = fwd->count;
		while (count > 0) {
			nread = (fwd->blocksize < count) ? fwd->blocksize : count;
			if ((rv = _forward_block(fwd->in_fd, fwd->out_fd, bp, nread, fwd->how)) <= 0) {
				break;
			}
			count -= nread;
		}
	}

	/* close fd */
	close(fwd->in_fd);
	close(fwd->out_fd);

	/* release */
	if (bp != buf) {
		free(bp);
	}
	if (rv) {
		return (void *)!NULL;
	}
	return NULL;
}

/**
* Initializes 'forwarding' struct with values.
*
* @param fwd	the struct to populate
* @param to_join	sets to_join member
* @param in_fd	sets in_fd member
* @param out_fd	sets out_fd member
* @param count	sets count member
* @param blocksize	sets blocksize member
* @param how	sets the how member
*/
void
russ_forwarding_init(struct russ_forwarding *fwd, int to_join, int in_fd, int out_fd, int count, int blocksize, int how) {
	fwd->to_join = to_join;
	fwd->in_fd = in_fd;
	fwd->out_fd = out_fd;
	fwd->count = count;
	fwd->blocksize = blocksize;
	fwd->how = how;
}

/**
* Forward bytes for n pairs of fds.
*
* @param nfwds	# of pairs
* @param fwds	array of pairs (with other info)
* @return	0 on success; -1 on failure
*/
int
russ_forward_bytes(int nfwds, struct russ_forwarding *fwds) {
	pthread_attr_t	attr;
	int		i;

	/* set up/start threads */
	for (i = 0; i < nfwds; i++) {
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, (1<<20)+(1<<20));
		if (pthread_create(&(fwds[i].th), &attr, _forward_bytes, (void *)&(fwds[i])) < 0) {
			goto kill_threads;
		}
		pthread_attr_destroy(&attr);
	}
	/* join threads */
	for (i = 0; i < nfwds; i++) {
		if (fwds[i].to_join) {
			pthread_join(fwds[i].th, NULL);
		}
	}
	return 0;
kill_threads:
	for (i--; i >= 0; i++) {
		pthread_cancel(fwds[i].th);
	}
	return -1;
}
