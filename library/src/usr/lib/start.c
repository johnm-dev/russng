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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <russ/priv.h>

/**
* Signal handler to reap spawned child servers.
*
* The child process is killed using the pgid which the reaper
* handles by this very handler. The kill can only be trigged
* once.
*/
static void
__reap_sigh(int signum) {
	static int	called = 0;

	if (called == 0) {
		kill(-getpgid(0), SIGTERM);
		called = 1;
	}
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
	char	outb[1024], *outp;
	int	pipefd[2];
	int	ev, pid, status;

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
	if ((waitpid(pid, &status, 0) < 0)
		|| (read(pipefd[0], outb, sizeof(outb)) < 0)) {
		outp = NULL;
	} else {
		outp = strdup(outb);
	}
	close(pipefd[0]);
	return outp;
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
	int			main_pgid;
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

	main_addr = russ_conf_get(conf, "main", "addr", "");
	if (strcmp(main_addr, "") == 0) {
		main_addr = russ_free(main_addr);
	} else {
		char	*tmp = NULL;

		tmp = main_addr;
		main_addr = russ_spath_resolve(tmp);
		tmp = russ_free(tmp);
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
		main_addr = strdup(tmppath);
		xargc = russ_sarray0_count(xargv, 128);
	}

	main_pgid = russ_conf_getint(conf, "main", "pgid", -1);
	if (main_pgid >= 0) {
		//setsid();
		setpgid(getpid(), main_pgid);
	}

	if (fork() == 0) {
		char	pidst[16];

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

		sprintf(pidst, "%d", pid);
		execlp("rureap", "rureap", pidst, main_addr, NULL);
#if 0
		/*
		* stay alive until child exits/is killed
		* kill process group to clean up
		*/
		signal(SIGHUP, __reap_sigh);
		signal(SIGINT, __reap_sigh);
		signal(SIGTERM, __reap_sigh);
		signal(SIGQUIT, __reap_sigh);

		waitpid(pid, &status, 0);
		remove(main_addr);
#endif
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
