/*
* rustart.c
*/

/*
# license--start
#
# Copyright 2014 John Marshall
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

#include <ctype.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <russ.h>

void
print_usage(char *prog_name) {
	printf(
"usage: rustart (-f <path>|-c <name>=<value>) [...] [-- ...]\n"
"\n"
"Start a uss server. Using the configuration, a socket file is\n"
"created and the listener socket is passed to the server.\n"
"\n"
"Where:\n"
"-f <path>\n"
"	Load configuration file.\n"
"-c <name>=<value>\n"
"	Set configuration attribute.\n"
"-- ...	Arguments to pass to the server program.\n"
);
}

struct russ_conf	*conf = NULL;

gid_t
group2gid(char *group) {
	struct group	*gr;
	gid_t	gid;

	if ((group) && (isdigit(group[0]))) {
		if (sscanf(group, "%d", &gid) < 1) {
			gid = -1;
		}
	} else {
		gid = ((gr = getgrnam(group)) == NULL) ? -1 : gr->gr_gid;
	}
	return (gid >= 0) ? gid : -1;
}

uid_t
user2uid(char *user) {
	struct passwd	*pw;
	uid_t		uid;

	if ((user) && (isdigit(user[0]))) {
		if (sscanf(user, "%d", &uid) < 1) {
			uid = -1;
		}
	} else {
		uid = ((pw = getpwnam(user)) == NULL) ? -1 : pw->pw_uid;
	}
	return (uid >= 0) ? uid : -1;
}

int
main(int argc, char **argv) {
	struct russ_lis	*lis;
	char		*addr, *path;
	char		*file_user, *file_group;
	int		file_mode;
	int		file_uid, file_gid;
	int		hide_conf;
	char		**oargv;
	int		oargc;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv[0]);
		exit(0);
	}
	/* duplicate args and load conf */
	oargc = argc;
	if ((oargv = russ_sarray0_dup(argv, oargc+1)) == NULL) {
		fprintf(stderr, "error: cannot duplicate argument list\n");
		exit(1);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot load configuration.\n");
		exit(1);
	}

	signal(SIGPIPE, SIG_IGN);

	/* get settings */
	path = russ_conf_get(conf, "server", "path", NULL);
	addr = russ_conf_get(conf, "server", "addr", NULL);
	file_mode = russ_conf_getsint(conf, "server", "file_mode", 0666);
	if ((file_user = russ_conf_get(conf, "server", "file_user", NULL)) == NULL) {
		file_uid = getuid();
	} else {
		file_uid = user2uid(file_user);
	}
	if ((file_group = russ_conf_get(conf, "server", "file_group", NULL)) == NULL) {
		file_uid = getgid();
	} else {
		file_gid = group2gid(file_group);
	}
	hide_conf = russ_conf_getint(conf, "server", "hide_conf", 0);

	argv[0] = path;
	if ((argv[0] == NULL) || ((lis = russ_announce(addr, file_mode, file_uid, file_gid)) == NULL)) {
		fprintf(stderr, "error: cannot set up server\n");
		exit(1);
	}
	/* listen socket is at fd lis->sd */
	execv(argv[0], hide_conf ? argv : oargv);
	fprintf(stderr, "error: cannot start server\n");
	exit(1);
}
