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

#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "russ/priv.h"

#define POLLHEN		(POLLHUP|POLLERR|POLLNVAL)
#define POLLIHEN	(POLLIN|POLLHUP|POLLERR|POLLNVAL)

/*
* Create NULL-terminated argv array from variadic argument list of
* "char *". The argv array does not allocate new memory for the
* individual items, but points to the strings in the va_list. To
* provide flexibility, an initial size of argv is specified to allow
* for some argv items to be set after return.
*
* @param maxargc	maximum size of argv
* @param iargc		initial size of argv (number of leading, unassigned)
* @param fargc		pointer to final size of argv array, NULL excluded
* @param ap		va_list
* @return		NULL-terminated argv array
*/
static char **
__russ_variadic_to_argv(int maxargc, int iargc, int *fargc, va_list ap) {
	va_list	ap2;
	char	**argv;
	int	i;

	va_copy(ap2, ap);
	for (i = iargc; (va_arg(ap2, char *) != NULL) && (i < maxargc); i++);
	va_end(ap2);

	if (i == maxargc) {
		return NULL;
	}
	if ((argv = malloc(sizeof(char *)*(i+1))) == NULL) {
		return NULL;
	}

	*fargc = i;
	va_copy(ap2, ap);
	for (i = iargc; i < *fargc; i++) {
		argv[i] = va_arg(ap, char*);
	}
	va_end(ap2);
	argv[i] = NULL;
	return argv;
}

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
* Wrapper for russ_dial with "execute" operation.
*
* @see russ_dialv()
*/
struct russ_cconn *
russ_execv(russ_deadline deadline, const char *spath, char **attrv, char **argv) {
	return russ_dialv(deadline, "execute", spath, attrv, argv);
}

/**
* execv helper corresponding to russ_dialv_timeout().
*
* @param timeout	timeout value
*
* @see russ_dialv_timeout()
*/
struct russ_cconn *
russ_execv_timeout(int timeout, const char *spath, char **attrv, char **argv) {
	return russ_dialv_timeout(timeout, "execute", spath, attrv, argv);
}

/**
* execv helper corresponding to russ_dialv_wait().
*
* @see russ_dialv_wait()
*/
int
russ_execv_wait(russ_deadline deadline, const char *spath, char **attrv, char **argv, int *exitst) {
	return russ_dialv_wait(deadline, "execute", spath, attrv, argv, exitst);
}

/**
* execv helper corresponding to russ_dialv_wait_timeout().
*
* @param timeout	timeout value
*
* @see russ_dialv_wait_timeout()
*/
int
russ_execv_wait_timeout(int timeout, const char *spath, char **attrv, char **argv, int *exitst) {
	return russ_dialv_wait_timeout(timeout, "execute", spath, attrv, argv, exitst);
}

/**
* execv helper corresponding to russ_dialv_wait_inouterr().
*
* @see russ_dialv_wait_inouterr()
*/
int
russ_execv_wait_inouterr(russ_deadline deadline, const char *spath, char **attrv, char **argv,
	int *exitst, struct russ_buf **rbufs) {
	return russ_dialv_wait_inouterr(deadline, "execute", spath, attrv, argv, exitst, rbufs);
}

/**
* execv helper corresponding to russ_dialv_wait_inouterr_timeout().
*
* @param timeout	timeout value
*
* @see russ_dialv_wait_inouterr_timeout()
*/
int
russ_execv_wait_inouterr_timeout(int timeout, const char *spath, char **attrv, char **argv,
	int *exitst, struct russ_buf **rbufs) {
	return russ_dialv_wait_inouterr_timeout(timeout, "execute", spath, attrv, argv, exitst, rbufs);
}

/**
* Wrapper for russ_dial with "execute" operation.
*
* @see russ_diall()
*/
struct russ_cconn *
russ_execl(russ_deadline deadline, const char *spath, char **attrv, ...) {
	struct russ_cconn	*cconn = NULL;
	va_list			ap;
	char			**argv = NULL;
	int			argc;

	va_start(ap, attrv);
	argv = __russ_variadic_to_argv(RUSS_REQ_ARGS_MAX, 0, &argc, ap);
	va_end(ap);
	if (argv == NULL) {
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

/**
* Make directories. For rustart() only.
*
* @param dirnames	:-separated list of paths
* @param mode		file mode
* @return		0 on success; -1 on failure
*/
static int
_mkdirs(char *dirnames, int mode) {
	struct stat	st;
	char		*dname = NULL;
	char		*p = NULL, *q = NULL;

	if ((dirnames = strdup(dirnames)) == NULL) {
		return -1;
	}

	/* test q before p */
	for (q = dirnames, p = q; (q != NULL) && (*p != '\0'); p = q+1) {
		if ((q = strchr(p, ':')) != NULL) {
			*q = '\0';
		}
		dname = russ_spath_resolve(p);
		if (stat(dname, &st) < 0) {
			if (mkdir(dname, mode) < 0) {
				goto fail;
			}
		} else if ((S_ISDIR(st.st_mode))
			&& ((st.st_mode & 0777) != mode)) {
			/* conflicting mode */
			goto fail;
		}
	}
	free(dirnames);
	free(dname);
	return 0;
fail:
	free(dirnames);
	free(dname);
	return -1;
}

/**
* Spawn server using arguments as provided from the command line and
* return a dynamically created socket file. Configuration and
* non-configuration (i.e., after the --) may be provided.
*
* @param argc		number of arguments
* @param argv		argument list
* @return		path to socket file (free by caller); NULL
*			on failure
*/
char *
russ_spawn(int argc, char **argv) {
	struct russ_conf	*conf = NULL;
	struct stat		st;
	char			**xargv = NULL;
	int			xargc;
	char			*main_addr = NULL;
	char			tmppath[PATH_MAX];
	char			tmparg[128];
	int			pid, status;
	int			timeout;
	int			i;

	/* duplicate args and load conf */
	xargc = argc;
	if ((xargv = russ_sarray0_dup(argv, argc+1)) == NULL) {
		fprintf(stderr, "error: cannot duplicate argument list\n");
		goto fail;
	} else if ((argc < 2) || ((conf = russ_conf_load(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot load configuration.\n");
		goto fail;
	}

	if (((main_addr = russ_conf_get(conf, "main", "addr", NULL)) != NULL)
		&& (strcmp(main_addr, "") == 0)) {
		main_addr = russ_free(main_addr);
	}
	if (main_addr == NULL) {
		if ((russ_snprintf(tmppath, sizeof(tmppath), "/tmp/.russ-%d-XXXXXX", getpid()) < 0)
			|| (mkstemp(tmppath) < 0)) {
			goto fail;
		}
		if ((russ_snprintf(tmparg, sizeof(tmparg), "main:addr=%s", tmppath) < 0)
			|| (russ_sarray0_append(&xargv, "-c", tmparg, NULL) < 0)) {
			remove(tmppath);
			goto fail;
		}
		xargc = russ_sarray0_count(xargv, 128);
		main_addr = strdup(tmppath);
	}

	if (fork() == 0) {
		setsid();

		/* close and reopen to occupy fds 0-2 */
		for (i = 0; i < 1024; i++) {
			close(i);
		}
		open("/dev/null", O_WRONLY);
		open("/dev/null", O_RDONLY);
		open("/dev/null", O_RDONLY);

		if ((pid = fork()) == 0) {
			signal(SIGPIPE, SIG_IGN);
			russ_start(xargc, xargv);
			exit(1);
		}
		waitpid(pid, &status, 0);
		remove(main_addr);
		exit(0);
	}
	for (timeout = 5000; timeout > 0; timeout -= 1) {
		if ((stat(main_addr, &st) == 0)
			&& (S_ISSOCK(st.st_mode))) {
			break;
		}
		usleep(1000);
	}
	if (timeout < 0) {
		goto fail;
	}

	conf = russ_conf_free(conf);
	xargv = russ_sarray0_free(xargv);

	return main_addr;

fail:
	conf = russ_conf_free(conf);
	xargv = russ_sarray0_free(xargv);
	main_addr = russ_free(main_addr);

	return NULL;
}

/**
* Wrapper for russ_spawn() supporting variadic args.
*
* @see russ_spawn()
*
* @param arg		argument 0 of NULL-terminated list of strings
* @return		NULL or russ_spawn()
*/
char *
russ_spawnl(char *dummy, ...) {
	va_list	ap;
	char	*saddr = NULL;
	char	**argv = NULL;
	int	argc;

	va_start(ap, dummy);
	argv = __russ_variadic_to_argv(RUSS_REQ_ARGS_MAX, 1, &argc, ap);
	va_end(ap);
	if (argv == NULL) {
		return NULL;
	}

	/* init argv[0], but it will be replaced */
	argv[0] = "";
	saddr = russ_spawn(argc, argv);
	/* only free argv */
	free(argv);
	return saddr;
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
	struct russ_conf	*conf = NULL;
	int			lisd;
	int			oargc;
	char			**oargv = NULL;
	char			*main_path = NULL, *main_addr = NULL;
	char			*main_cwd = NULL;
	mode_t			main_file_mode;
	char			*main_file_user = NULL, *main_file_group = NULL;
	int			main_hide_conf;
	char			*main_mkdirs = NULL;
	int			main_mkdirs_mode;
	char			*main_user = NULL, *main_group = NULL;
	mode_t			main_umask;
	uid_t			file_uid, uid;
	gid_t			file_gid, gid;
	int			i;

	/* duplicate args and load conf */
	oargc = argc;
	if ((oargv = russ_sarray0_dup(argv, oargc+1)) == NULL) {
		fprintf(stderr, "error: cannot duplicate argument list\n");
		exit(1);
	} else if ((argc < 2) || ((conf = russ_conf_load(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot load configuration.\n");
		exit(1);
	}

	/* get settings */
	main_path = russ_conf_get(conf, "main", "path", NULL);
	main_addr = russ_conf_get(conf, "main", "addr", NULL);
	main_cwd = russ_conf_get(conf, "main", "cwd", "/");
	main_umask = (mode_t)russ_conf_getsint(conf, "main", "umask", 022);
	main_file_mode = russ_conf_getsint(conf, "main", "file_mode", 0666);
	file_uid = (main_file_user = russ_conf_get(conf, "main", "file_user", NULL)) \
		? russ_user2uid(main_file_user) : getuid();
	file_gid = (main_file_group = russ_conf_get(conf, "main", "file_group", NULL)) \
		? russ_group2gid(main_file_group) : getgid();
	uid = (main_user = russ_conf_get(conf, "main", "user", NULL)) \
		? russ_user2uid(main_user) : getuid();
	gid = (main_group = russ_conf_get(conf, "main", "group", NULL)) \
		? russ_group2gid(main_group) : getgid();
	main_hide_conf = russ_conf_getint(conf, "main", "hide_conf", 0);
	main_mkdirs = russ_conf_get(conf, "main", "mkdirs", NULL);
	main_mkdirs_mode = russ_conf_getsint(conf, "main", "mkdirs_mode", 0755);

	/* close fds >= 3 */
	for (i = 3; i < 1024; i++) {
		close(i);
	}

	/* set up */
	umask(main_umask);
	if (chdir(main_cwd) < 0) {
		fprintf(stderr, "error: cannot change directory\n");
		exit(1);
	}

	/* change uid/gid then exec; listen socket is at fd lisd */
	if (russ_switch_userinitgroups(uid, gid) < 0) {
		fprintf(stderr, "error: cannot switch user\n");
		exit(1);
	}

	/* check for server program */
	if ((main_path == NULL)
		|| (access(main_path, R_OK|X_OK))) {
		fprintf(stderr, "error: cannot access server program\n");
		exit(1);
	}

	/* create directories */
	if (main_mkdirs) {
		if (_mkdirs(main_mkdirs, main_mkdirs_mode) < 0) {
			fprintf(stderr, "error: cannot make directories\n");
			exit(1);
		}
	}

	/* set up socket */
	if ((lisd = russ_announce(main_addr, main_file_mode, file_uid, file_gid)) < 0) {
		fprintf(stderr, "error: cannot set up socket\n");
		exit(1);
	}

	/* exec server itself */
	argv[0] = main_path;
	if (execv(argv[0], main_hide_conf ? argv : oargv) < 0) {
		fprintf(stderr, "error: cannot exec server\n");
		exit(1);
	}

	return -1;
}

/**
* Wrapper for russ_start supporting variadic args.
*
* @see russ_start()
*
* @param dummy		ignored placeholder
* @return		-1 on failure or russ_start()
*/
int
russ_startl(char *dummy, ...) {
	va_list			ap;
	char			**argv = NULL;
	int			argc;

	va_start(ap, dummy);
	argv = __russ_variadic_to_argv(RUSS_REQ_ARGS_MAX, 0, &argc, ap);
	va_end(ap);
	if (argv == NULL) {
		return -1;
	}

	russ_start(argc, argv);

	/* should not get here; clean up on failure */
	free(argv);
	return -1;
}
