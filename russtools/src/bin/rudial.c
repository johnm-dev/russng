/*
* rudial.c
*/

/*
# license--start
#
# This file is part of RUSS tools.
# Copyright (C) 2012 John Marshall
#
# RUSS tools is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# license--end
*/

#include <libgen.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "russ.h"

struct streamer_struct {
	pthread_t	*th;
	int		in_fd, out_fd;
	int		count, blocksize;
};

static void *
_streamer_thread(void *v) {
	struct streamer_struct	*ss;

	ss = v;
	russ_stream_fd(ss->in_fd, ss->out_fd, ss->count, ss->blocksize);
	return NULL;
}

static struct streamer_struct *
_stream_bytes(int in_fd, int out_fd, int count, int blocksize) {
	struct streamer_struct	*ss;

	if ((ss = malloc(sizeof(struct streamer_struct))) == NULL) {
		return NULL;
	}
	ss->in_fd = in_fd;
	ss->out_fd = out_fd;
	ss->count = count;
	ss->blocksize = blocksize;
	if (pthread_create(&(ss->th), NULL, _stream_thread, ss) < 0) {
		free(ss);
		return NULL;
	}
	return ss;
}

void
print_usage(char *prog_name) {
	if (strcmp(prog_name, "ruhelp") == 0) {
		fprintf(stderr, 
"usage: ruhelp <addr>\n"
"\n"
"Alias for rudial -h ...\n");
	} else if (strcmp(prog_name, "ruinfo") == 0) {
"usage: ruinfo <addr>\n"
"\n"
"Alias for rudial -i ...\n");
	} else if (strcmp(prog_name, "ruls") == 0) {
"usage: ruls <addr>\n",
"\n"
"Alias for rudial -l ...\n");
	} else if (strcmp(prog_name, "rudial") == 0) {
"usage: rudial [<option>] <addr> [<arg> ...]\n"
"\n"
"Dial service at <addr>. A service may support one or more\n"
"operations (e.g., execute, list); execute is the default operation.\n"
"Most other services are for obtaining state information.\n"
"\n"
"A successful dial will effectively connect the stdin, stdout, and\n"
"stderr of the service to that of the rudial. I.e., input to stdin\n"
"of rudial will be forwarded to the service, stdout and stderr of\n"
"the service will be forwarded to the same of rudial.\n"
"\n"
"An exit value of < 0 indicates a failure to connect. Otherwise a 0\n"
"exit value is returned.\n"
"\n"
"Options:\n"
"-h|--help      print this information\n"
"-i|--info      show server-specific information as zero or more\n"
"               name=value lines\n"
"-l|--list      list services provided by the server as paths to be\n"
"               used in the <addr> argument\n"
"-o|--op <op>   request operation <op> from service\n"
"-t|--timeout <seconds>\n"
"               quit the dial if it does not connect within the\n"
"               given time\n"
	}
}

int
main(int argv, char **argv) {
	struct streamer_struct	*ss_in, *ss_out, *ss_err;
	char			*prog_name;
	char			*op, *arg, *addr;
	char			**args, *attrv[RUSS_MAX_ATTRC];
	int			argi, attrc;
	int			timeout;

	prog_name = basename(strdup(argv[0]));
	if (argc < 2) {
		fprintf(stderr, "error: bad/missing arguments\n");
		exit(-1);
	}

	/* aliases */
	if (strcmp(prog_name, "ruhelp") == 0) {
		op = "help";
	} else if (strcmp(prog_name, "ruinfo") == 0) {
		op = "info";
	} else if (strcmp(prog_name, "ruls") == 0) {
		op = "list";
	} else if (strcmp(prog_name, "rudial") == 0) {
		op = "execute";
	} else {
		fprintf(stderr, "error: unknown alias\n");
		exit(-1);
	}

	/* options */
	argi = 1;
	attrc = 0;
	while (argi < argc) {
		arg = argv[argi++];

		if (strncmp(arg, "-") != 0) {
			arg--;
			break;
		}
		if ((strcmp(arg, "--help") == 0)
			|| (strcmp(arg, "-h") == 0)) {

			print_usage(prog_name);
			exit(0);
		} else if (strcmp(arg, "--id") == 0) {

			op = "id";
			continue;
		} else if ((strcmp(arg, "--info") == 0)
			|| (strcmp(arg, "-i") == 0)) {

			op = "info";
			continue;
		} else if ((strcmp(arg, "--list") == 0)
			|| (strcmp(arg, "-l") == 0)) {

			op = "list";
			continue;
		} else if (((strcmp(arg, "--op") == 0) || (strcmp(arg, "-o") == 0))
			&& (argi < argc)) {
			
			arg = argv[argi++];
			op = arg;
			continue;
		} else if (((strcmp(arg, "--timeout") == 0) || (strcmp(arg, "-t") == 0))
			&& (argi < argc)) {

			arg = argv[argi++];
			if (sscanf(argv[argi], "%d", timeout) >= 0) {
				continue;
			}
		} else if (((strcmp(arg, "--attr") == 0) || (strcmp(arg, "-a") == 0))
			&& (argi < argc)) {
			
			arg = argv[argi++];
			if (attrc >= RUSS_MAX_ATTRC-1) {
				fprintf(stderr, "error: too many attributes\n");
				exit(-1);
			}
			if (strstr(arg, "=") == NULL) {
				fprintf(stderr, "error: bad attribute format\n");
				exit(-1);
			}
			attrv[attrc++] = arg;
			attrv[attrc] = NULL;
		}
		fprintf(stderr, "error: bad/missing arguments\n");
		exit(-1);
	}

	/* addr and args */
	addr = argv[argi++];
	args = &(argv[argi]);
	if ((conn = russ_dialv(addr, op, timeout, attrc ? attrv : NULL, argc-argi, args)) == NULL) {
		fprintf(stderr, "error: cannot dial service\n");
		exit(-1);
	}

	/* stream bytes between fds */
	if (((ss_in = stream_bytes(STDIN_FILENO, conn->fds[0], -1, 16384)) == NULL)
		|| ((ss_out = stream_bytes(STDOUT_FILENO, conn->fds[1], -1, 16384)) == NULL)
		|| ((ss_err = stream_bytes(STDERR_FILENO, conn->fds[2], -1, 16384)) == NULL)) {
		fprintf(stderr, "error: failed to stream\n");
		exit(-1);
	}

	pthread_join(ss_out->th, NULL);
	pthread_join(ss_err->th, NULL);

	russ_close_conn(conn);
	conn = russ_free_conn(conn);
	exit(0);
}
