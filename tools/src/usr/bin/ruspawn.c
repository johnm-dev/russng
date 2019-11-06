/*
* ruspawn.c
*/

/*
# license--start
#
# Copyright 2017 John Marshall
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <russ/russ.h>

struct russ_conf	*conf = NULL;

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
* Spawn server using arguments as provided from the command line and
* return a dynamically created socket file. Configuration and
* non-configuration (i.e., after the --) may be provided.
*
* @param argc		number of arguments
* @param argv		argument list
* @return		path to socket file (free by caller); NULL
*			on failure
*/
static char *
__spawn(int argc, char **argv) {
	struct russ_conf	*conf = NULL;
	struct stat		st;
	char			**xargv = NULL;
	int			xargc;
	char			*main_addr = NULL;
	int			main_pgid;
	char			buf[16];
	char			tmppath[PATH_MAX];
	char			tmparg[128];
	int			pid, reappid, status;
	int			timeout;
	int			withpids = 0;
	int			notifyfds[2];
	int			i;

	/* special handling of --withpids */
	if (strcmp(argv[1], "--withpids") == 0) {
		withpids = 1;
		russ_sarray0_remove(argv, 1);
		argc--;
	}

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

	/* to synchronize server creation and connect */
	pipe(notifyfds);

	if ((reappid = fork()) == 0) {
		char	pidst[16];

		/* close and reopen to occupy fds 0-2 */
		russ_close_range(0, 2);
		open("/dev/null", O_RDONLY);
		open("/dev/null", O_WRONLY);
		open("/dev/null", O_WRONLY);

		russ_close(notifyfds[0]);

		if ((pid = fork()) == 0) {
			signal(SIGPIPE, SIG_IGN);
			russ_start(xargc, xargv, notifyfds[1]);
			exit(1);
		}

		russ_close(notifyfds[1]);

		sprintf(pidst, "%d", pid);
#if 0
		execlp("rureap", "rureap", pidst, main_addr, NULL);
#endif
#if 1
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
#endif
	}

	/* wait for notification */
	russ_close(notifyfds[1]);
	read(notifyfds[0], buf, 1);

	conf = russ_conf_free(conf);
	xargv = russ_sarray0_free(xargv);

	if (withpids) {
		printf("%d:%d:", reappid, main_pgid);
	}

	return main_addr;

fail:
	conf = russ_conf_free(conf);
	xargv = russ_sarray0_free(xargv);
	main_addr = russ_free(main_addr);

	return NULL;
}

void
print_usage(char *prog_name) {
	printf(
"usage: ruspawn (-f <path>|-c <name>=<value>) [...] [-- ...]\n"
"\n"
"Spawn a russ server. Using the configuration, a socket file is\n"
"created and the listener socket is passed to the server. The path\n"
"the socket file is output to stdout.\n"
"\n"
"ruspawn is different from rustart in the following ways. If\n"
"main:addr (the socket file path) is not specified, a path is\n"
"dynamically chosen and used to set main:addr. A reaper process is\n"
"started to automatically cleanup the socket file when the server\n"
"exits. If the server or the repear are signaled, they both will be\n"
"terminated and the socket file cleaned up.\n"
"\n"
"ruspawn is the preferred way to start a server.\n"
"\n"
"Where:\n"
"-c <name>=<value>\n"
"	Set configuration attribute.\n"
"-f <path>\n"
"	Load configuration file.\n"
"-- ...	Arguments to pass to the server program.\n"
);
}

int
main(int argc, char **argv) {
	char	*saddr;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv[0]);
		exit(0);
	}
	if ((saddr = __spawn(argc, argv)) == NULL) {
		fprintf(stderr, "error: cannot spawn server\n");
		exit(1);
	}
	printf("%s", saddr);
	exit(0);
}
