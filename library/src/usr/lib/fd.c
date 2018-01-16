/*
* lib/fd.h
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <russ/priv.h>

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
* Close range of fds (inclusive).
*
* @param fdlow		starting fd
* @param fdhi		ending fd; -1 to indicate OPEN_MAX
*/
void
russ_close_range(int fdlow, int fdhi) {
	int	fd, fdmax;

	fdmax = sysconf(_SC_OPEN_MAX);
	if (fdlow > fdmax) {
		return;
	}
	if (fdhi == -1) {
		fdhi = fdmax;
	} else if (fdhi > fdmax) {
		fdhi = fdmax;
	}
	for (fd = fdlow; fd <= fdhi; fd++) {
		while ((close(fd) < 0) && (errno == EINTR));
	}
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
	void	*bend = NULL;
	ssize_t	n;

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
	void		*bend = NULL;
	int		rv, due_time;
	ssize_t		n;

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
* Test an fd status using poll events.
*
* @param fd		file descriptor
* @param events		poll events
* @return		poll() result; -1 on failure
*/
int
russ_test_fd(int fd, int events) {
	struct pollfd	pollfds[1];
	int		rv;

	pollfds[0].fd = fd;
	pollfds[0].events = events;
	rv = poll(pollfds, 1, 0);
	return (rv < 0) ? rv : pollfds[0].revents;
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
	void	*bend = NULL;
	ssize_t	n;

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
	void		*bend = NULL;
	int		rv, due_time;
	ssize_t		n;

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
* Initialize descriptor array to value.
*
* @param self		descriptor array
* @param count		size of fds array
* @param value		initialization value
*/
void
russ_fds_init(int *self, int count, int value) {
	int	i;

	for (i = 0; i < count; i++) {
		self[i] = value;
	}
}

/**
* Close descriptors in array and set to -1.
*
* @param self		descriptor array
* @param count		size of fds array
*/
void
russ_fds_close(int *self, int count) {
	int	i;

	for (i = 0; i < count; i++) {
		if (self[i] > -1) {
			russ_close(self[i]);
			self[i] = -1;
		}
	}
}

/**
* Make pipes and store passed arrays.
*
* Pipes are created with read and write descriptors in stored to
* separate arrays. A failure releases all created pipes.
*
* @param count		# of pipes to make; minimum size of rfds and wfds
* @param[out] rfds	array for created read fds
* @param[out] wfds	array for created write fds 
* @return		0 on success; -1 on error
*/
int
russ_make_pipes(int count, int *rfds, int *wfds) {
	int	i, pfds[2];

	russ_fds_init(rfds, count, -1);
	russ_fds_init(wfds, count, -1);

	for (i = 0; i < count; i++) {
		if (count == 3) {
			if (socketpair(AF_UNIX, SOCK_STREAM, 0, pfds) < 0) {
				goto close_fds;
			}
		} else {
			if (pipe(pfds) < 0) {
				goto close_fds;
			}
		}
		rfds[i] = pfds[0];
		wfds[i] = pfds[1];
	}
	return 0;

close_fds:
	russ_fds_close(rfds, i);
	russ_fds_close(wfds, i);
	return -1;
}

/**
* poll() with automatic restart on EINTR.
*
* @param deadline	deadline to complete operation
* @param pollfds	array of pollfd
* @param nfds		# of descriptors in pollfds
* @return		0 if deadline passed; otherwise, value as returned by system poll
*/
int
russ_poll_deadline(russ_deadline deadline, struct pollfd *pollfds, int nfds) {
	int	timeout;
	int	rv;

	while (1) {
//fprintf(stderr, "russ_poll rv (%d) errno (%d)\n", rv, errno);
		if ((timeout = russ_to_timeout(deadline)) == 0) {
			/* timeout */
			rv = 0;
			break;
		}
		if (((rv = poll(pollfds, nfds, timeout)) >= 0)
			|| (errno != EINTR)) {
			/* data (>0), timeout (0), non-EINTR error */
			break;
		}
	}
//fprintf(stderr, "pollfds (%d) nfds (%d) poll_timeout (%d)\n", pollfds[0], nfds, poll_timeout);
//fprintf(stderr, "russ_poll rv (%d) errno (%d)\n", rv, errno);
	return rv;
}
