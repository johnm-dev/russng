/*
** lib/helpers.c
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

#include <stdarg.h>
#include <stdlib.h>

#include "russ_priv.h"

#define POLLHEN	(POLLHUP|POLLERR|POLLNVAL)

/*
* Convert variadic argument list of "char *" to argv array.
*
* @param[out] argc	size of argv array
* @param ap		va_list for counting args
* @param ap2		va_list for creating argv array
* @return		argv array
*/
static char **
__russ_variadic_to_argv(int *argc, va_list ap, va_list ap2) {
	char	**argv;
	int	i;

	/* count args */
	for (i = 0; va_arg(ap, char *) != NULL ; i++);
	va_end(ap);

	/* create argv */
	if ((argv = malloc(sizeof(char *)*i)) == NULL) {
		return NULL;
	}
	*argc = i;
	for (i = 0; i < *argc; i++) {
		argv[i] = va_arg(ap2, char *);
	}
	va_end(ap2);

	return argv;
}

/**
* Helper to dial and wait for exit value. I/O for connection stdin,
* stdout, and stderr comes from/goes to 0 capacity buffers
* effectively mimicking /dev/null.
*
* @param[out] exit_status
*			exit status returned
* @return		0 for success; -1 for failure
*
* @see russ_dialv()
*/
int
russ_dialv_wait(russ_deadline deadline, char *op, char *spath, char **attrv, char **argv, int *exit_status) {
	struct russ_buf	*rbufs[3];
	int		ev, rv;
	int		i;

	for (i = 0; i < 3; i++) {
		rbufs[i] = russ_buf_new(0);
	}

	rv = russ_dialv_wait_inouterr(deadline, op, spath, attrv, argv, exit_status, (struct russ_buf **)&rbufs);

	for (i = 0; i < 3; i++) {
		rbufs[i] = russ_buf_free(rbufs[i]);
	}

	return rv;
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
* @param[out] exit_status
*			exit status returned
* @return		0 for success; -1 for failure
*
* @see russ_dialv()
*/
int
russ_dialv_wait_inouterr(russ_deadline deadline, char *op, char *spath, char **attrv, char **argv,
	int *exit_status, struct russ_buf **rbufs) {
	struct russ_conn	*conn;
	struct pollfd		pollfds[4];
	char			*buf, dbuf[1<<16];
	int			fd, openfds, rv;
	int			i, n;

	if ((conn = russ_dialv(deadline, op, spath, attrv, argv)) == NULL) {
		return -1;
	}

	pollfds[0].fd = conn->fds[0];
	pollfds[0].events = POLLOUT;
	pollfds[1].fd = conn->fds[1];
	pollfds[1].events = POLLIN;
	pollfds[2].fd = conn->fds[2];
	pollfds[2].events = POLLIN;
	pollfds[3].fd = conn->fds[3];
	pollfds[3].events = POLLIN;
	openfds = 4;

	while ((openfds > 0) && ((rv = russ_poll(pollfds, 4, deadline)) >= 0)) {
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
					if ((n == 0) || (n = russ_read(fd, buf, n)) < 0) {
						goto close_fd;
					}
					if (rbufs[i]->cap > 0) {
						rbufs[i]->len += n;
					}
				} else if (pollfds[i].revents & POLLOUT) {
					n = rbufs[i]->len-rbufs[i]->off;
					buf = &rbufs[i]->data[rbufs[i]->off];
					if ((n == 0) || (n = russ_write(fd, buf, n)) < 0) {
						goto close_fd;
					}
					rbufs[i]->off += n;
				} else if (pollfds[i].revents & POLLHEN) {
close_fd:
					russ_fds_close(&fd, 1);
					conn->fds[i] = -1;
					pollfds[i].fd = -1;
					openfds--;
				}
			}
		}
		if (pollfds[3].revents & POLLIN) {
			russ_conn_wait(conn, exit_status, deadline);
			openfds--;
		}
	}
	russ_conn_close(conn);
	return 0;
}

/**
* Temporary function to simplify pyruss binding for
* russ_dialv_wait_inouterr()
*
* Note: this function should not be used from C.
*/
int
russ_dialv_wait_inouterr3(russ_deadline deadline, char *op, char *spath, char **attrv, char **argv,
	int *exit_status, struct russ_buf *stdin, struct russ_buf *stdout, struct russ_buf *stderr) {
	struct russ_buf	*rbufs[3];

	rbufs[0] = stdin;
	rbufs[1] = stdout;
	rbufs[2] = stderr;

	return russ_dialv_wait_inouterr(deadline, op, spath, attrv, argv, exit_status, rbufs);
}

/**
* Wrapper for russ_dial with "execute" operation.
*
* @see russ_dialv()
*/
struct russ_conn *
russ_execv(russ_deadline deadline, char *spath, char **attrv, char **argv) {
	return russ_dialv(deadline, "execute", spath, attrv, argv);
}

/**
* Wrapper for russ_dial with "execute" operation.
*
* @see russ_diall()
*/
struct russ_conn *
russ_execl(russ_deadline deadline, char *spath, char **attrv, ...) {
	struct russ_conn	*conn;
	va_list			ap;
	char			**argv;
	int			argc;

	va_start(ap, attrv);
	if ((argv = __russ_variadic_to_argv(&argc, ap, ap)) == NULL) {
		return NULL;
	}
	conn = russ_dialv(deadline, "execute", spath, attrv, argv);
	free(argv);

	return conn;
}

/**
* Wrapper for russ_dial with "help" operation.
*
* @param deadilne	deadline to complete operation
* @param spath		service path
* @return		connection object
*/
struct russ_conn *
russ_help(russ_deadline deadline, char *spath) {
	return russ_dialv(deadline, "help", spath, NULL, NULL);
}

/**
* Wrapper for russ_dial with "info" operation.
*
* @param deadline	deadline to complete operation
* @param spath		service path
* @return		connection object
*/
struct russ_conn *
russ_info(russ_deadline deadline, char *spath) {
	return russ_dialv(deadline, "info", spath, NULL, NULL);
}

/**
* Wrapper for russ_dial with "list" operation.
*
* @param deadline	deadline to complete operation
* @param spath		service path
* @return		connection object
*/
struct russ_conn *
russ_list(russ_deadline deadline, char *spath) {
	return russ_dialv(deadline, "list", spath, NULL, NULL);
}