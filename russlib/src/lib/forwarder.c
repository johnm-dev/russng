/*
** forwarder.c
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
* The actual byte forwarder code.
*
* @param fwd	forwarder object
* @return	NULL on success, !NULL on failure
*/
static void *
_forward_bytes(void *_fwd) {
	struct russ_forwarder	*fwd;
	char			buf[1<<20], *bp;
	long			nread, nwrite, count;
	int			rv = 0;

	/* setup */
	fwd = (struct russ_forwarder *)_fwd;
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
* Initializes forwarder struct with values.
*
* @param fwd	forwarder object
* @param in_fd	sets in_fd member
* @param out_fd	sets out_fd member
* @param count	sets count member
* @param blocksize	sets blocksize member
* @param how	sets the how member
*/
void
russ_forwarder_init(struct russ_forwarder *self, int in_fd, int out_fd, int count, int blocksize, int how) {
	self->in_fd = in_fd;
	self->out_fd = out_fd;
	self->count = count;
	self->blocksize = blocksize;
	self->how = how;
}

/**
* Forward bytes for n pairs of fds.
*
* @param nfwds	# of pairs
* @param fwds	array of pairs (with other info)
* @return	0 on success; -1 on failure
*/
int
russ_run_forwarders(int nfwds, struct russ_forwarder *fwds) {
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
	return 0;
kill_threads:
	for (i--; i >= 0; i++) {
		pthread_cancel(fwds[i].th);
	}
	return -1;
}

/**
* Start forwarder thread.
*
* @param self	forwarder object
* @return	0 on success; -1 on failure
*/
int
russ_forwarder_start(struct russ_forwarder *self) {
	pthread_attr_t	attr;

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, (1<<20)*2);
	if (pthread_create(&(self->th), &attr, _forward_bytes, (void *)self) < 0) {
		return -1;
	}
	pthread_attr_destroy(&attr);
	return 0;
}

/**
* Join forwarder thread.
*
* @param self	forwarder object
*/
int
russ_forwarder_join(struct russ_forwarder *self) {
	pthread_join(self->th, NULL);
}
