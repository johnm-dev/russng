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

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

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

/* See qsort man page */
static int
_russ_start_mkdirs_cmpstringp(const void *p1, const void *p2) {
   return strcmp(* (char * const *) p1, * (char * const *) p2);
}

/**
* Make directories specified in a section.
*
* The format is:
*     <path>=[<uid>]:[<gid>]:<mode>
*
* The ordering of creation is undefined.
*
* @param conf		configuration object
* @param secname	section name
* @return		0 on success; -1 on failure
*/
static int
_russ_start_mkdirs(struct russ_conf *conf, char *secname) {
	struct stat	st;
	char		*value = NULL;
	char		**paths = NULL, *path = NULL;
	char		group[32], user[32];
	uid_t		uid, gid;
	mode_t		mode;
	int		i, npaths;

	user[31] = '\0';
	group[31] = '\0';

	if (!russ_conf_has_section(conf, secname)) {
		return 0;
	}

	if ((paths = russ_conf_options(conf, secname)) == NULL) {
		return 0;
	}
	for (npaths = 0; paths[npaths] != NULL; npaths++);
	qsort(path, npaths, sizeof(char *), _russ_start_mkdirs_cmpstringp);

	for (i = 0; paths[i] != NULL; i++) {
		path = paths[i];
		if ((path[0] != '/')
			|| ((value = russ_conf_get(conf, secname, path, NULL)) == NULL)
			|| (sscanf(value, "%31[^:]:%31[^:]:%i", user, group, &mode) != 3)) {
			goto cleanup;
		}

		if ((strcmp(user, "") == 0) || (strcmp(user, "-1") == 0)) {
			uid = getuid();
		} else if (russ_user2uid(user, &uid) < 0) {
			goto cleanup;
		}
		if ((strcmp(group, "") == 0) || (strcmp(group, "-1") == 0)) {
			gid = getgid();
		} else if (russ_group2gid(group, &gid) < 0) {
			goto cleanup;
		}

		if (((mkdir(path, mode) < 0) && (errno != EEXIST))
			|| (stat(path, &st) < 0)
			|| (!S_ISDIR(st.st_mode))) {
			goto cleanup;
		}
		if (chmod(path, mode) < 0) {
			/* try to restore it */
			chmod(path, st.st_mode);
			goto cleanup;
		}
		if (chown(path, uid, gid) < 0) {
			/* no backout! */
			goto cleanup;
		}
	}

	value = russ_free(value);
	paths = russ_sarray0_free(paths);
	return 0;

cleanup:
	value = russ_free(value);
	paths = russ_sarray0_free(paths);
	return -1;
}

/**
* Set environment variables found in the a named section. The section
* name is typically main.env.
*
* The items are iterated through in order.
*
* @param conf		configuration object
* @param secname	section name
* @return		0 on success; -1 on failure
*/
static int
_russ_start_setenvs(struct russ_conf *conf, char *secname) {
	char	**names = NULL, *name = NULL;
	char	*value = NULL, *rvalue = NULL;
	int	i, rv;

	if (russ_env_reset() < 0) {
		return -1;
	}

	if (!russ_conf_has_section(conf, secname)) {
		return 0;
	}

	if ((names = russ_conf_options(conf, secname)) == NULL) {
		return -1;
	}

	for (rv = 0, i = 0; (names[i] != NULL) && (rv == 0); i++) {
		name = names[i];

		value = russ_conf_get(conf, secname, name, "");
		rvalue = russ_env_resolve(value);
		rv = setenv(name, rvalue, 1);
		value = russ_free(value);
		rvalue = russ_free(rvalue);
	}

	russ_conf_sarray0_free(names);
	if (rv < 0) {
		return -1;
	}
	return 0;
}

/**
* Set single resource (soft, hard) found in the named section.
*
* @param conf		configuration object
* @param secname	section name
* @param name		limit name in section
* @return		0 on success; -1 on failure
*/
static int
_russ_start_setlimit(struct russ_conf *conf, char *secname, char *limitname) {
	struct rlimit	rlim;
	char		*endptr = NULL, *hard = NULL, *soft = NULL;
	char		hardname[32], softname[32];
	int		resource;

	/* match limitname and resource (if existent) */
	if (strcmp(limitname, "as") == 0) {
#ifndef RLIMIT_AS
		return 0;
#else
		resource = RLIMIT_AS;
#endif
	} else if (strcmp(limitname, "core") == 0) {
#ifndef RLIMIT_CORE
		return 0;
#else
		resource = RLIMIT_CORE;
#endif
	} else if (strcmp(limitname, "cpu") == 0) {
#ifndef RLIMIT_CPU
		return 0;
#else
		resource = RLIMIT_CPU;
#endif
	} else if (strcmp(limitname, "data") == 0) {
#ifndef RLIMIT_DATA
		return 0;
#else
		resource = RLIMIT_DATA;
#endif
	} else if (strcmp(limitname, "fsize") == 0) {
#ifndef RLIMIT_FSIZE
		return 0;
#else
		resource = RLIMIT_FSIZE;
#endif
	} else if (strcmp(limitname, "memlock") == 0) {
#ifndef RLIMIT_MEMLOCK
		return 0;
#else
		resource = RLIMIT_MEMLOCK;
#endif
	} else if (strcmp(limitname, "nofile") == 0) {
#ifndef RLIMIT_NOFILE
		return 0;
#else
		resource = RLIMIT_NOFILE;
#endif
	} else if (strcmp(limitname, "nproc") == 0) {
#ifndef RLIMIT_NPROC
		return 0;
#else
		resource = RLIMIT_NPROC;
#endif
	} else if (strcmp(limitname, "rss") == 0) {
#ifndef RLIMIT_RSS
		return 0;
#else
		resource = RLIMIT_RSS;
#endif
	} else if (strcmp(limitname, "stack") == 0) {
#ifndef RLIMIT_STACK
		return 0;
#else
		resource = RLIMIT_STACK;
#endif
	} else {
		// unknown
		return 0;
	}

	if ((russ_snprintf(softname, sizeof(softname), "%s.soft", limitname) < 0)
		|| (russ_snprintf(hardname, sizeof(hardname), "%s.hard", limitname) < 0)) {
		goto fail;
	}
	soft = russ_conf_get(conf, secname, softname, NULL);
	hard = russ_conf_get(conf, secname, hardname, NULL);

	if (RUSS_DEBUG__russ_start_setlimit) {
		fprintf(stderr, "RUSS_DEBUG__russ_start_setlimit: name (%s) soft (%s) hard (%s)\n", limitname, soft, hard);
	}

	/* get and update rlimit */
	getrlimit(resource, &rlim);
	if ((soft == NULL) || (strcmp(soft, "") == 0)) {
		// keep current
	} else if (strcmp(soft, "unlimited") == 0) {
		rlim.rlim_cur = RLIM_INFINITY;
	} else {
		rlim.rlim_cur = strtol(soft, NULL, 10);
		if (*endptr != '\0') {
			goto fail;
		}
	}
	if ((hard == NULL) || (strcmp(hard, "") == 0)) {
		// keep current
	} else if (strcmp(hard, "unlimited") == 0) {
		rlim.rlim_max = RLIM_INFINITY;
	} else {
		rlim.rlim_max = strtol(hard, NULL, 10);
		if (*endptr != '\0') {
			goto fail;
		}
	}

	if (RUSS_DEBUG__russ_start_setlimit) {
		fprintf(stderr, "RUSS_DEBUG__russ_start_setlimit: name (%s) rlim (%ld:%ld)\n", limitname, rlim.rlim_cur, rlim.rlim_max);
	}

	soft = russ_free(soft);
	hard = russ_free(hard);
	return setrlimit(resource, &rlim);
fail:
	soft = russ_free(soft);
	hard = russ_free(hard);
	return -1;
}

/**
* Set process limits found in the named section.
*
* Some limits may not be settable on the platform.
*
* @param conf		configuration object
* @param secname	section name
* @return		0 on success; -1 on failure
*/
static int
_russ_start_setlimits(struct russ_conf *conf, char *secname) {
	if ((_russ_start_setlimit(conf, secname, "as") < 0)
		|| (_russ_start_setlimit(conf, secname, "rss") < 0)
		|| (_russ_start_setlimit(conf, secname, "data") < 0)
		|| (_russ_start_setlimit(conf, secname, "stack") < 0)
		|| (_russ_start_setlimit(conf, secname, "memlock") < 0)

		|| (_russ_start_setlimit(conf, secname, "core") < 0)
		|| (_russ_start_setlimit(conf, secname, "cpu") < 0)
		|| (_russ_start_setlimit(conf, secname, "fsize") < 0)
		//|| (_russ_start_setlimit(conf, secname, "locks") < 0)
		|| (_russ_start_setlimit(conf, secname, "nofile") < 0)
		|| (_russ_start_setlimit(conf, secname, "nproc") < 0)) {
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
			"-c", "main:addr=",
			"-c", "main:closeonaccept=1",
			"-c", "main:accepttimeout=5000",
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
* @param starttype	start server by start or spawn mechanism
* @param conf		russ_conf object
* @return		start string ([<reappid>:<pgid>:]<main_addr>) or NULL on failure
*/
char *
russ_start(int starttype, struct russ_conf *conf) {
	int			lisd;
	int			largc;
	char			**largv = NULL;
	char			*filename;
	char			*main_launcher = NULL, **main_launcher_items = NULL;
	char			*main_path = NULL, *main_addr = NULL;
	char			*main_cwd = NULL;
	mode_t			main_file_mode;
	char			*main_file_user = NULL, *main_file_group = NULL;
	int			main_pgid;
	char			*main_user = NULL, *main_group = NULL;
	mode_t			main_umask;
	char			buf[128], tmparg[128];
	uid_t			file_uid, uid;
	gid_t			file_gid, gid;
	int			conffd;
	int			i, pos;

	/* get settings */
	if ((main_launcher = russ_conf_get(conf, "main", "launcher", NULL)) != NULL) {
		main_launcher_items = russ_sarray0_new_split(main_launcher, ":", 0);
		main_launcher = russ_free(main_launcher);

		for (i = 0; main_launcher_items[i] != NULL; i++) {
			main_launcher = main_launcher_items[i];
			if (access(main_launcher, R_OK|X_OK) == 0) {
				if ((main_launcher = strdup(main_launcher_items[i])) == NULL) {
					fprintf(stderr, "error: out of memory\n");
					return NULL;
				}
				break;
			}
			main_launcher = NULL;
		}
		if (main_launcher == NULL) {
			fprintf(stderr, "error: cannot find launcher\n");
			return NULL;
		}
	}
	main_launcher_items = russ_sarray0_free(main_launcher_items);

	if (starttype == RUSS_STARTTYPE_START) {
		if ((main_addr = russ_conf_get(conf, "main", "addr", NULL)) == NULL) {
			/* fatal: no cleanup done */
			return NULL;
		}
	} else if (starttype == RUSS_STARTTYPE_SPAWN) {
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
			/*
			* create/reserve tmp file for socket file in one of a
			* few possible locations
			*/
			if ((main_addr = russ_mkstemp(NULL)) == NULL) {
				goto fail;
			}

			if (russ_conf_set2(conf, "main", "addr", main_addr) < 0) {
				remove(main_addr);
				goto fail;
			}
		}
	}

	main_path = russ_conf_get(conf, "main", "path", NULL);
	main_pgid = russ_conf_getint(conf, "main", "pgid", -1);

	main_cwd = russ_conf_get(conf, "main", "cwd", "/");
	main_umask = (mode_t)russ_conf_getint(conf, "main", "umask", 022);
	main_file_mode = russ_conf_getint(conf, "main", "file_mode", 0666);

	if ((main_file_user = russ_conf_get(conf, "main", "file_user", NULL)) != NULL) {
		if (russ_user2uid(main_file_user, &file_uid) < 0) {
			goto fail;
		}
	} else {
		file_uid = getuid();
	}
	if ((main_file_group = russ_conf_get(conf, "main", "file_group", NULL)) != NULL) {
		if (russ_group2gid(main_file_group, &file_gid) < 0) {
			goto fail;
		}
	} else {
		file_gid = getgid();
	}
	if ((main_user = russ_conf_get(conf, "main", "user", NULL)) != NULL) {
		if (russ_user2uid(main_user, &uid) < 0) {
			goto fail;
		}
	} else {
		uid = getuid();
	}
	if ((main_group = russ_conf_get(conf, "main", "group", NULL)) != NULL) {
		if (russ_group2gid(main_group, &gid) < 0) {
			goto fail;
		}
	} else {
		gid = getgid();
	}

	/* close fds > 2 */
	russ_close_range(3, -1);

	/* change pgid */
	if (main_pgid >= 0) {
		//setsid();
		setpgid(getpid(), main_pgid);
	}

	/* change uid/gid then exec; listen socket is at fd lisd */
	if (russ_switch_userinitgroups(uid, gid) < 0) {
		fprintf(stderr, "error: cannot switch user\n");
		return NULL;
	}

	umask(main_umask);

	if (chdir(main_cwd) < 0) {
		fprintf(stderr, "error: cannot change directory\n");
		return NULL;
	}

	/* check for server program */
	if ((main_path == NULL)
		|| (access(main_path, R_OK|X_OK))) {
		fprintf(stderr, "error: cannot access server program\n");
		return NULL;
	}

	/* create directories */
	if (_russ_start_mkdirs(conf, "main.dirs") < 0) {
		fprintf(stderr, "error: cannot make directories\n");
		return NULL;		
	}

	/* set limits */
	if (_russ_start_setlimits(conf, "main.limits") < 0) {
		fprintf(stderr, "error: cannot set limits\n");
		return NULL;
	}

	/* set env */
	if (_russ_start_setenvs(conf, "main.env") < 0) {
		fprintf(stderr, "error: cannot set environment\n");
		return NULL;
	}

	/* announce */
	if ((lisd = russ_announce(main_addr, main_file_mode, file_uid, file_gid)) < 0) {
		fprintf(stderr, "error: cannot set up socket\n");
		return NULL;
	}

	if (starttype == RUSS_STARTTYPE_SPAWN) {
		int	pid, status;

		/*
		* process tree:
		*   ruspawn -> returns addr path
		*     ruspawn/reaper
		*       server
		*/
		if ((pid = fork()) != 0) {
			char	startstr[2048];

			/* parent */
			close(lisd);

			if (russ_snprintf(startstr, sizeof(startstr), "%d:%d:%s", pid, getpgid(getpid()), main_addr) < 0) {
				return NULL;
			}
			return strdup(startstr);
		} else {
			/* child */
			/* close and reopen to occupy fds 0-2 */
			russ_close_range(0, 2);
			open("/dev/null", O_RDONLY);
			open("/dev/null", O_WRONLY);
			open("/dev/null", O_WRONLY);

			if ((pid = fork()) != 0) {
				/* child-parent */
				close(lisd);

				/*
				* stay alive until child exits/is killed
				* kill process group to clean up
				*/
				signal(SIGPIPE, SIG_IGN);
				signal(SIGHUP, __reap_sigh);
				signal(SIGINT, __reap_sigh);
				signal(SIGTERM, __reap_sigh);
				signal(SIGQUIT, __reap_sigh);

				waitpid(pid, &status, 0);
				remove(main_addr);
				exit(0);
			} else {
				/* child-child */
			}
		}
	}

	/* RUSS_STARTTYPE_SPAWN child or RUSS_STARTTYPE_START */

	russ_snprintf(buf, sizeof(buf), "%d", lisd);
	russ_conf_set2(conf, "main", "sd", buf);

	if ((filename = russ_mkstemp(NULL)) == NULL) {
		goto fail;
	}
	if ((russ_conf_write(conf, filename) < 0)
		|| ((conffd = open(filename, O_RDONLY)) < 0)) {
		remove(filename);
		goto fail;
	}
	remove(filename);

	largc = 0;
	largv = russ_sarray0_new(0, NULL);
	if (main_launcher) {
		if (russ_sarray0_append(&largv, main_launcher, NULL) < 0) {
			goto fail;
		}
	}
	if ((russ_sarray0_append(&largv, main_path, NULL) < 0)
		|| (russ_snprintf(buf, sizeof(buf), "%d", conffd) < 0)
		|| (russ_sarray0_append(&largv, "--fd", buf, NULL) < 0)) {
		goto fail;
	}
	largc = russ_sarray0_count(largv, 128);

	execv(largv[0], largv);

fail:
	/* should not get here */
	fprintf(stderr, "error: cannot exec server\n");
	conf = russ_conf_free(conf);
	largv = russ_sarray0_free(largv);

	return NULL;
}
