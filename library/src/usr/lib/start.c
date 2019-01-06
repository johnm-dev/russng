/*
* lib/start.c
*/

/*
# license--start
#
# Copyright 2018 John Marshall
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
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <russ/priv.h>

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
* Augment path arguments passed to rustart by making them absolute
* based on the CWD.
*
* @param argc		number of arguments
* @param argv		argument list
* @return		0 on success; -1 on failure
*/
static int
_russ_start_augment_path(int argc, char **argv) {
	char	buf[1024], cwd[1024];
	char	*p = NULL;
	int	i;

	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		return -1;
	}
	for (i = 1; i < argc; i++) {
		p = argv[i];
		if (strcmp(p, "-c") == 0) {
			i++;
			if (i >= argc) {
				return -1;
			}
		} else if (strcmp(p, "-f") == 0) {
			i++;
			if (i >= argc) {
				return -1;
			}
			if ((strncmp(argv[i], "./", 2) == 0)
				|| (strncmp(argv[i], "../", 3) != 0)
				|| (argv[i][0] != '/')) {
				/* relative */
				if (russ_snprintf(buf, sizeof(buf), "%s/%s", cwd, argv[i]) < 0) {
					return -1;
				}
				/* TODO: should argv[i] be freed? */
				argv[i] = strdup(buf);
			}
		}
	}
	return 0;
}

/**
* Set single resource (soft, hard).
*
* @param conf		configuration object
* @param name		option name in "main.limits" section
* @return		0 on success; -1 on failure
*/
int
_russ_start_setlimit(struct russ_conf *conf, char *name) {
	struct rlimit	rlim;
	char		*sh, *soft, *hard, *endptr;
	int		resource;

	/* match name and resource (if existent) */
	if (strcmp(name, "as") == 0) {
#ifndef RLIMIT_AS
		return 0;
#else
		resource = RLIMIT_AS;
#endif
	} else if (strcmp(name, "core") == 0) {
#ifndef RLIMIT_CORE
		return 0;
#else
		resource = RLIMIT_CORE;
#endif
	} else if (strcmp(name, "cpu") == 0) {
#ifndef RLIMIT_CPU
		return 0;
#else
		resource = RLIMIT_CPU;
#endif
	} else if (strcmp(name, "data") == 0) {
#ifndef RLIMIT_DATA
		return 0;
#else
		resource = RLIMIT_DATA;
#endif
	} else if (strcmp(name, "fsize") == 0) {
#ifndef RLIMIT_FSIZE
		return 0;
#else
		resource = RLIMIT_FSIZE;
#endif
	} else if (strcmp(name, "memlock") == 0) {
#ifndef RLIMIT_MEMLOCK
		return 0;
#else
		resource = RLIMIT_MEMLOCK;
#endif
	} else if (strcmp(name, "nofile") == 0) {
#ifndef RLIMIT_NOFILE
		return 0;
#else
		resource = RLIMIT_NOFILE;
#endif
	} else if (strcmp(name, "nproc") == 0) {
#ifndef RLIMIT_NPROC
		return 0;
#else
		resource = RLIMIT_NPROC;
#endif
	} else if (strcmp(name, "rss") == 0) {
#ifndef RLIMIT_RSS
		return 0;
#else
		resource = RLIMIT_RSS;
#endif
	} else if (strcmp(name, "stack") == 0) {
#ifndef RLIMIT_STACK
		return 0;
#else
		resource = RLIMIT_STACK;
#endif
	} else {
		// unknown
		return 0;
	}

	/* get setting if present */
	if ((sh = russ_conf_get(conf, "main.limits", name, NULL)) == NULL) {
		return 0;
	}
	soft = sh;
	if ((hard = strchr(sh, ':')) != NULL) {
		*hard = '\0';
		hard++;
	}
	if (getenv("RUSS_DEBUG__russ_start_setlimit")) {
		fprintf(stderr, "RUSS_DEBUG__russ_start_setlimit: name (%s) soft (%s) hard (%s)\n", name, soft, hard);
	}

	/* get and update rlimit */
	getrlimit(resource, &rlim);
	if (strcmp(soft, "") == 0) {
		// keep current
	} else if (strcmp(soft, "unlimited") == 0) {
		rlim.rlim_cur = RLIM_INFINITY;
	} else {
		rlim.rlim_cur = strtol(soft, &endptr, 10);
		if (*endptr != '\0') {
			goto fail;
		}
	}
	if (hard) {
		if (strcmp(hard, "") == 0) {
			// keep current
		} else if (strcmp(hard, "unlimited") == 0) {
			rlim.rlim_max = RLIM_INFINITY;
		} else {
			rlim.rlim_max = strtol(hard, &endptr, 10);
			if (*endptr != '\0') {
				goto fail;
			}
		}
	}
	if (getenv("RUSS_DEBUG__russ_start_setlimit")) {
		fprintf(stderr, "RUSS_DEBUG__russ_start_setlimit: name (%s) rlim (%ld:%ld)\n", name, rlim.rlim_cur, rlim.rlim_max);
	}

	sh = russ_free(sh);
	return setrlimit(resource, &rlim);
fail:
	sh = russ_free(sh);
	return -1;
}

/**
* Set process limits.
*
* Some limits may not be settable on the platform.
*
* @param conf		configuration object
* @return		0 on success; -1 on failure
*/
int
_russ_start_setlimits(struct russ_conf *conf) {
	if ((_russ_start_setlimit(conf, "as") < 0)
		|| (_russ_start_setlimit(conf, "rss") < 0)
		|| (_russ_start_setlimit(conf, "data") < 0)
		|| (_russ_start_setlimit(conf, "stack") < 0)
		|| (_russ_start_setlimit(conf, "memlock") < 0)

		|| (_russ_start_setlimit(conf, "core") < 0)
		|| (_russ_start_setlimit(conf, "cpu") < 0)
		|| (_russ_start_setlimit(conf, "fsize") < 0)
		//|| (_russ_start_setlimit(conf, "locks") < 0)
		|| (_russ_start_setlimit(conf, "nofile") < 0)
		|| (_russ_start_setlimit(conf, "nproc") < 0)) {
		return -1;
	}
	return 0;
}

/**
* Spawn server using the "ruspawn" tool.
*
* Using the tool has the advantage of a small process footprint and
* no ties to the calling process.
*
* @param caddr		configuration file
* @return		path to created socket file
*/
char *
russ_ruspawn(char *caddr) {
	char	outb[1024], *outp = NULL;
	int	pipefd[2];
	int	ev, n, pid, status;

	if (pipe(pipefd) < 0) {
		return NULL;
	}

	if ((pid = fork()) == 0) {
		/* TODO: close unneeded fds to avoid leaks */
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		close(STDERR_FILENO);
		open("/dev/null", O_WRONLY);

		execlp("ruspawn", "ruspawn",
			"-f", caddr,
			"-c", "main:closeonaccept=1",
			"-c", "main:accepttimeout=2500",
			NULL);
		exit(1);
	}
	close(pipefd[1]);
	if (waitpid(pid, &status, 0) < 0) {
		outp = NULL;
	} else {
		n = read(pipefd[0], outb, sizeof(outb));
		if ((n < 0) || (n == sizeof(outb))) {
			outp = NULL;
		} else {
			outb[n] = '\0';
			outp = strdup(outb);
		}
	}
	close(pipefd[0]);
	return outp;
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
russ_start(int argc, char **argv, int notifyfd) {
	struct russ_conf	*conf = NULL;
	int			lisd;
	int			largc;
	char			**largv = NULL;
	char			*main_path = NULL, *main_addr = NULL;
	char			*main_cwd = NULL;
	mode_t			main_file_mode;
	char			*main_file_user = NULL, *main_file_group = NULL;
	int			main_hide_conf;
	char			*main_mkdirs = NULL;
	int			main_mkdirs_mode;
	char			*main_user = NULL, *main_group = NULL;
	mode_t			main_umask;
	char			buf[128];
	uid_t			file_uid, uid;
	gid_t			file_gid, gid;
	int			i, pos;

	/* get local copy of args */
	largc = argc;
	if ((largv = russ_sarray0_dup(argv, largc+1)) == NULL) {
		fprintf(stderr, "error: cannot duplicate argument list\n");
		exit(1);
	}
	/* load conf */
	if ((argc < 2) || ((conf = russ_conf_load(&argc, argv)) == NULL)) {
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

	/* close fds >= 3, except for notifyfd */
	if (notifyfd < 3) {
		russ_close_range(3, -1);
	} else {
		russ_close_range(3, notifyfd-1);
		russ_close_range(notifyfd+1, -1);
	}

	/* change uid/gid then exec; listen socket is at fd lisd */
	if (russ_switch_userinitgroups(uid, gid) < 0) {
		fprintf(stderr, "error: cannot switch user\n");
		exit(1);
	}

	umask(main_umask);

	if (chdir(main_cwd) < 0) {
		fprintf(stderr, "error: cannot change directory\n");
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

	/* set limits */
	if (_russ_start_setlimits(conf) < 0) {
		fprintf(stderr, "error: cannot set limits\n");
		exit(1);
	}

	/* announce */
	if ((lisd = russ_announce(main_addr, main_file_mode, file_uid, file_gid)) < 0) {
		fprintf(stderr, "error: cannot set up socket\n");
		exit(1);
	}

	russ_close(notifyfd);

	/* pass listening socket description as config arguments */
	russ_snprintf(buf, sizeof(buf), "main:sd=%d", lisd);
	pos = russ_sarray0_find(largv, "--");
	pos = (pos < 0) ? largc : pos;
	russ_sarray0_insert(&largv, pos, "-c", buf, NULL);
	largc += 2;

	/* exec server itself */
	if (execv(main_path, main_hide_conf ? argv : largv) < 0) {
		fprintf(stderr, "error: cannot exec server\n");
		exit(1);
	}

	largv = russ_sarray0_free(largv);

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

	russ_start(argc, argv, -1);

	/* should not get here; clean up on failure */
	free(argv);
	return -1;
}
