/*
* lib/helpers.c
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
russ_dialv_wait(russ_deadline deadline, const char *op, const char *spath, char **attrv, char **argv, int *exit_status) {
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
russ_dialv_wait_inouterr(russ_deadline deadline, const char *op, const char *spath, char **attrv, char **argv,
	int *exit_status, struct russ_buf **rbufs) {
	struct russ_cconn	*cconn;
	struct pollfd		pollfds[4];
	char			*buf, dbuf[1<<16];
	int			fd, openfds, rv;
	int			i, n;

	if ((cconn = russ_dialv(deadline, op, spath, attrv, argv)) == NULL) {
		return -1;
	}

	pollfds[0].fd = cconn->fds[0];
	pollfds[0].events = POLLOUT;
	pollfds[1].fd = cconn->fds[1];
	pollfds[1].events = POLLIN;
	pollfds[2].fd = cconn->fds[2];
	pollfds[2].events = POLLIN;
	pollfds[3].fd = cconn->fds[3];
	pollfds[3].events = POLLIN;
	openfds = 4;

	while ((openfds > 0) && ((rv = russ_poll_deadline(deadline, pollfds, 4)) >= 0)) {
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
					cconn->fds[i] = -1;
					pollfds[i].fd = -1;
					openfds--;
				}
			}
		}
		if (pollfds[3].revents & POLLIN) {
			russ_cconn_wait(cconn, deadline, exit_status);
			openfds--;
		}
	}
	russ_cconn_close(cconn);
	return 0;
}

/**
* Temporary function to simplify pyruss binding for
* russ_dialv_wait_inouterr()
*
* Note: this function should not be used from C.
*/
int
russ_dialv_wait_inouterr3(russ_deadline deadline, const char *op, const char *spath, char **attrv, char **argv,
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
struct russ_cconn *
russ_execv(russ_deadline deadline, const char *spath, char **attrv, char **argv) {
	return russ_dialv(deadline, "execute", spath, attrv, argv);
}

/**
* Wrapper for russ_dial with "execute" operation.
*
* @see russ_diall()
*/
struct russ_cconn *
russ_execl(russ_deadline deadline, const char *spath, char **attrv, ...) {
	struct russ_cconn	*cconn;
	va_list			ap;
	char			**argv;
	int			argc;

	va_start(ap, attrv);
	if ((argv = __russ_variadic_to_argv(&argc, ap, ap)) == NULL) {
		return NULL;
	}
	cconn = russ_dialv(deadline, "execute", spath, attrv, argv);
	argv = russ_free(argv);

	return cconn;
}

/**
* Wrapper for russ_dial with "help" operation.
*
* @param deadline	deadline to complete operation
* @param spath		service path
* @return		client connection object
*/
struct russ_cconn *
russ_help(russ_deadline deadline, const char *spath) {
	return russ_dialv(deadline, "help", spath, NULL, NULL);
}

/**
* Wrapper for russ_dial with "info" operation.
*
* @param deadline	deadline to complete operation
* @param spath		service path
* @return		client connection object
*/
struct russ_cconn *
russ_info(russ_deadline deadline, const char *spath) {
	return russ_dialv(deadline, "info", spath, NULL, NULL);
}

/**
* Wrapper for russ_dial with "list" operation.
*
* @param deadline	deadline to complete operation
* @param spath		service path
* @return		client connection object
*/
struct russ_cconn *
russ_list(russ_deadline deadline, const char *spath) {
	return russ_dialv(deadline, "list", spath, NULL, NULL);
}

/**
* Start a server using arguments as provide from the command line.
* Configuration and non-configuration (i.e., after the --) may be
* provided.
*
* @param argc		number of arguments
* @param argv		argument list
* @return		-1 on failure (which should not happen)
*/
int
russ_start(int argc, char **argv) {
	struct russ_lis		*lis;
	struct russ_conf	*conf;
	int			oargc;
	char			**oargv;
	char			*file_user, *file_group, *user, *group;
	char			*path, *addr;
	mode_t			file_mode;
	uid_t			file_uid, uid;
	gid_t			file_gid, gid;
	int			hide_conf;

	/* duplicate args and load conf */
	oargc = argc;
	if ((oargv = russ_sarray0_dup(argv, oargc+1)) == NULL) {
		fprintf(stderr, "error: cannot duplicate argument list\n");
		exit(1);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot load configuration.\n");
		exit(1);
	}

	/* get settings */
	path = russ_conf_get(conf, "server", "path", NULL);
	addr = russ_conf_get(conf, "server", "addr", NULL);
	file_mode = russ_conf_getsint(conf, "server", "file_mode", 0666);
	file_uid = (file_user = russ_conf_get(conf, "server", "file_user", NULL)) \
		? russ_user2uid(file_user) : getuid();
	file_gid = (file_group = russ_conf_get(conf, "server", "file_group", NULL)) \
		? russ_group2gid(file_group) : getgid();
	uid = (user = russ_conf_get(conf, "server", "user", NULL)) \
		? russ_user2uid(user) : getuid();
	gid = (group = russ_conf_get(conf, "server", "group", NULL)) \
		? russ_group2gid(group) : getgid();
	hide_conf = russ_conf_getint(conf, "server", "hide_conf", 0);

	argv[0] = path;
	if ((argv[0] == NULL) || ((lis = russ_announce(addr, file_mode, file_uid, file_gid)) == NULL)) {
		fprintf(stderr, "error: cannot set up server\n");
		exit(1);
	}
	/* listen socket is at fd lis->sd */
	setgid(gid);
	setuid(uid);
	execv(argv[0], hide_conf ? argv : oargv);
	return -1;
}