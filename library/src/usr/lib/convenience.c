/*
* lib/convenience.c
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

#include <poll.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "russ/priv.h"

#define POLLHEN		(POLLHUP|POLLERR|POLLNVAL)
#define POLLIHEN	(POLLIN|POLLHUP|POLLERR|POLLNVAL)

/**
* Timeout-based helper corresponding to russ_dialv().
*
* @param timeout	timeout value
*
* @see russ_dialv()
*/
struct russ_cconn *
russ_dialv_timeout(int timeout, const char *op, const char *spath, char **attrv, char **argv) {
	return russ_dialv(russ_to_deadline(timeout), op, spath, attrv, argv);
}

/**
* Helper to dial and wait for exit value. I/O for connection stdin,
* stdout, and stderr comes from/goes to 0 capacity buffers
* effectively mimicking /dev/null.
*
* @param[out] exitst	exit status returned
* @return		return value of russ_cconn_wait()
*
* @see russ_dialv()
*/
int
russ_dialv_wait(russ_deadline deadline, const char *op, const char *spath, char **attrv, char **argv, int *exitst) {
	struct russ_buf	*rbufs[3];
	int		ev, rv;
	int		i;

	for (i = 0; i < 3; i++) {
		rbufs[i] = russ_buf_new(0);
	}

	rv = russ_dialv_wait_inouterr(deadline, op, spath, attrv, argv, exitst, (struct russ_buf **)&rbufs);

	for (i = 0; i < 3; i++) {
		rbufs[i] = russ_buf_free(rbufs[i]);
	}

	return rv;
}

/**
* Timeout-based helper corresponding to russ_dialv_wait.
*
* @param timeout	timeout value
*
* @see russ_dialv_wait()
*/
int
russ_dialv_wait_timeout(int timeout, const char *op, const char *spath, char **attrv, char **argv, int *exitst) {
	return russ_dialv_wait(russ_to_deadline(timeout), op, spath, attrv, argv, exitst);
}

/**
* Helper to dial, auto perform I/O, and get exit value all in one.
* An object with the I/O data and exit value is returned.
*
* Note: receiving buffers (connectin stdout, stderr) with cap == 0
* do not capture but discard. This is helpful when only the exit
* status is important.
*
* @param[inout] rbufs	array of russ_buf objects to hold in, out, err
* @param[out] exitst	exit status returned
* @return		return value of russ_cconn_wait()
*
* @see russ_dialv()
*/
int
russ_dialv_wait_inouterr(russ_deadline deadline, const char *op, const char *spath, char **attrv, char **argv,
	int *exitst, struct russ_buf **rbufs) {
	struct russ_cconn	*cconn = NULL;
	struct pollfd		pollfds[4];
	char			*buf = NULL;
	char			dbuf[1<<16];
	int			fd, openfds, rv, wrv;
	int			i, n;

	/* default wait rv to general error */
	wrv = -1;

	if ((cconn = russ_dialv(deadline, op, spath, attrv, argv)) == NULL) {
		return wrv;
	}

	pollfds[0].fd = cconn->fds[0];
	pollfds[0].events = POLLOUT;
	pollfds[1].fd = cconn->fds[1];
	pollfds[1].events = POLLIN;
	pollfds[2].fd = cconn->fds[2];
	pollfds[2].events = POLLIN;
	pollfds[3].fd = cconn->sysfds[RUSS_CONN_SYSFD_EXIT];
	pollfds[3].events = POLLIN;
	openfds = 4;

	wrv = RUSS_WAIT_UNSET; /* not set */
	while ((openfds > 0) && ((rv = russ_poll_deadline(deadline, pollfds, 4)) > 0)) {
		for (i = 0; i < 3; i++) {
			if (pollfds[i].revents) {
				fd = pollfds[i].fd;

				if (pollfds[i].revents & POLLIN) {
					if (rbufs[i]->cap > 0) {
						n = rbufs[i]->cap-rbufs[i]->len;
						buf = &rbufs[i]->data[rbufs[i]->len];
					} else {
						n = sizeof(dbuf);
						buf = dbuf;
					}
					if ((n == 0)
						|| ((n = russ_read(fd, buf, n)) < 0)
						|| (n == 0)) {
						goto close_fd;
					}
					if (rbufs[i]->cap > 0) {
						rbufs[i]->len += n;
					}
				} else if (pollfds[i].revents & POLLOUT) {
					n = rbufs[i]->len-rbufs[i]->off;
					n = RUSS__MIN(n, 1<<16);
					buf = &rbufs[i]->data[rbufs[i]->off];
					if ((n == 0)
						|| ((n = russ_write(fd, buf, n)) < 0)
						|| (n == 0)) {
						goto close_fd;
					}
					rbufs[i]->off += n;
				} else if (pollfds[i].revents & POLLHEN) {
close_fd:
					russ_fds_close(&fd, 1);
					cconn->fds[i] = -1;
					pollfds[i].fd = -1;
					openfds--;
				}
			}
		}
		/* special case exit fd */
		if (pollfds[3].revents & POLLIHEN) {
			wrv = russ_cconn_wait(cconn, deadline, exitst);
			/* update in case of change */
			if ((pollfds[3].fd = cconn->sysfds[RUSS_CONN_SYSFD_EXIT]) == -1) {
				openfds--;
			}
		}
	}
	if ((rv == 0) && (wrv > RUSS_WAIT_OK)) {
		/* wait rv as expired deadline */
		wrv = RUSS_WAIT_TIMEOUT;
	}
	russ_cconn_close(cconn);
	return wrv;
}

/**
* Timeout-based helper corresponding to russ_dialv_wait_inouterr.
*
* @param timeout	timeout value
*
* @see russ_dialv_wait_inouterr()
*/
int
russ_dialv_wait_inouterr_timeout(int timeout, const char *op, const char *spath, char **attrv, char **argv,
	int *exitst, struct russ_buf **rbufs) {
	return russ_dialv_wait_inouterr(russ_to_deadline(timeout), op, spath, attrv, argv, exitst, rbufs);
}

/**
* Temporary function to simplify pyruss binding for
* russ_dialv_wait_inouterr()
*
* Note: this function should not be used from C.
*/
int
russ_dialv_wait_inouterr3(russ_deadline deadline, const char *op, const char *spath, char **attrv, char **argv,
	int *exitst, struct russ_buf *stdin, struct russ_buf *stdout, struct russ_buf *stderr) {
	struct russ_buf	*rbufs[3];

	rbufs[0] = stdin;
	rbufs[1] = stdout;
	rbufs[2] = stderr;

	return russ_dialv_wait_inouterr(deadline, op, spath, attrv, argv, exitst, rbufs);
}

/**
* Initialize server according to configuration settings. Returns
* a russ_svr object initialized with a default root russ_svcnode
* and NULL handler.
*
* @param conf		russ_conf object
* @return		russ_svr object; NULL on failure
*/
struct russ_svr *
russ_init(struct russ_conf *conf) {
	struct russ_svr		*svr = NULL;
	struct russ_svcnode	*root = NULL;
	int			sd;
	int			accepttimeout, closeonaccept;

	if (conf == NULL) {
		return NULL;
	}

	/* load debug variables */
	russ_debug_init();

	/* setup base server */
	sd = (int)russ_conf_getint(conf, "main", "sd", RUSS_SVR_LIS_SD_DEFAULT);
	accepttimeout = (int)russ_conf_getint(conf, "main", "accepttimeout", RUSS_SVR_TIMEOUT_ACCEPT);
	closeonaccept = (int)russ_conf_getint(conf, "main", "closeonaccept", 0);
	if (((root = russ_svcnode_new("", NULL)) == NULL)
		|| ((svr = russ_svr_new(root, 0, sd)) == NULL)
		|| (russ_svr_set_accepttimeout(svr, accepttimeout) < 0)
		|| (russ_svr_set_closeonaccept(svr, closeonaccept) < 0)) {
		goto fail;
	}
	return svr;
fail:
	root = russ_svcnode_free(root);
	svr = russ_svr_free(svr);
	return NULL;
}
