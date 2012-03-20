/*
** rudial.c
**
** front-end for russ services
*/

/*
# GPL--start
# This file is part of russ
# Copyright (C) 2011 Environment/Environnement Canada
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2
# of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
# GPL--end
*/

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "russ.h"

int	forwarding = 1;

int
list_dir(char *path) {
	DIR		*dir;
	struct dirent	*dirent;

	if ((dir = opendir(path)) == NULL) {
		return(-1);
	}

	while ((dirent = readdir(dir)) != NULL) {
		if (strncmp(dirent->d_name, ".", 1) != 0) {
			printf("%s\n", dirent->d_name);
		}
	}
	closedir(dir);
	return 0;
}

/* forward bytes between fds */
void
__forward_bytes(int *fds, int how) {
	char	buf[1<<20];
	int	rcount, wcount;
	int	src_fd, dst_fd, close_fd;

	/* unpack */
	src_fd = ((int *)fds)[0];
	dst_fd = ((int *)fds)[1];
	close_fd = ((int *)fds)[2];

	while (1) {
		if (how == 0) {
			rcount = russ_readn(src_fd, buf, sizeof(buf));
		} else {
			rcount = russ_readline(src_fd, buf, sizeof(buf));
		}
		if (rcount <= 0) {
			/* EOF or error */
			break;
		}
		wcount = russ_writen(dst_fd, buf, rcount);
		if (wcount != rcount) {
			/* error */
			break;
		}
		fsync(dst_fd);
	}
	if (close_fd > -1) {
		close(close_fd);
	}
	free(fds);
}

/* forward bytes between fds (pthread-friendly) */
void *
forward_bytes(void *fds) {
	__forward_bytes(fds, 0);
	return NULL;
}

/* forward lines between fds (pthread-friendly) */
void *
forward_lines(void *fds) {
	__forward_bytes(fds, 1);
	return NULL;
}

/* spawn thread to forward bytes */
int
spawn_forward_bytes(pthread_t *th, int src_fd, int dst_fd, int close_fd) {
	int	*fds;

	if ((fds = malloc(sizeof(int)*3)) == NULL) {
		return -1;
	}
	fds[0] = src_fd;
	fds[1] = dst_fd;
	fds[2] = close_fd;
	return pthread_create(th, NULL, forward_lines, (void *)fds);
}

void
print_usage(char **argv) {
	char	*name, *bname;

	name = strdup(argv[0]);
	bname = basename(name);

	if (strcmp(bname, "ruhelp") == 0) {
		fprintf(stderr,
"usage: ruhelp <addr>\n"
"\n"
"An alias for 'rudial -h ...'.\n");

	} else if (strcmp(bname, "ruinfo") == 0) {
		fprintf(stderr,
"usage: ruinfo -i <addr>\n"
"\n"
"An alias for 'rudial -i ...'.\n");

	} else if (strcmp(bname, "ruls") == 0) {
		fprintf(stderr,
"usage: ruls [<addr>]\n"
"\n"
"An alias for 'rudial -l ...'.\n");

	} else {
		fprintf(stderr,
"usage: rudial [<option>] <addr> [<arg> ...]\n"
"\n"
"Dial service at <addr> with given arguments.\n"
"\n"
"Where <option> is:\n"
"--execute	Execute service request (shortcut for '--op execute';\n"
"		default	if <option> is not given).\n"
"--id		Return unique server id.\n"
"-h|--help	Show help for the (sub)service.\n"
"-i|--info	Show name=value information for server.\n"
"-l|--list	List (sub)service(s) (shortcut for '--op list').\n"
"-o|--op <op>	Use operation name (e.g., execute) but not supported\n"
"		otherwise.\n"
"-t|--timeout <seconds>\n"
"		Allow timeout seconds to complete connection. Default\n"
"		is no timeout.\n"
"\n"
"Exit values of %d (local error) and %d (call error) are\n"
"reserved by rudial.\n", ERROR_DIAL_LOCAL, ERROR_DIAL_CALL);
	}
	free(name);
}

int
main(int argc, char **argv) {
	char			*prog_name;
	struct russ_conn	*conn;
	char			*addr, *saddr, *spath, *op;
	int			exit_value;
	pthread_t		in_th, out_th, err_th;
	int			pass_fds[3];
	struct pipe_fds		pipes[3];
	int			argi;
	int			line_buffering;
	int			timeout;

	prog_name = basename(strdup(argv[0]));

	if ((argc < 2) && (strcmp(prog_name, "ruls") != 0)) {
		print_usage(argv);
		exit(ERROR_DIAL_LOCAL);
	}

	if (strcmp(prog_name, "ruhelp") == 0) {
		op = "help";
		argi = 1;
	} else if (strcmp(prog_name, "ruinfo") == 0) {
		op = "info";
		argi = 1;
	} else if (strcmp(prog_name, "ruls") == 0) {
		op = "list";
		argi = 1;
	} else {
		/* TODO: arg parsing does not handle multiple options properly */
		/* rudial */
		timeout = -1;
		if ((strcmp(argv[1], "--op") == 0)
			|| (strcmp(argv[1], "-o") == 0)) {
			if (argc < 3) {
				fprintf(stderr, "error: missing op\n");
				exit(ERROR_DIAL_LOCAL);
			} else {
				op = argv[2];
			}
			argi = 3;
		} else if (strncmp(argv[1], "-", 1) == 0) {
			if (strcmp(argv[1], "--execute") == 0) {
				op = "execute";
			} else if ((strcmp(argv[1], "--help") == 0)
				|| (strcmp(argv[1], "-h") == 0)) {
				op = "help";
			} else if ((strcmp(argv[1], "--info") == 0)
				|| (strcmp(argv[1], "-i") == 0)) {
				op = "info";
			} else if (strcmp(argv[1], "--id") == 0) {
				op = "id";
			} else if ((strcmp(argv[1], "--list") == 0)
				|| (strcmp(argv[1], "-l") == 0)) {
				op = "list";
			} else if (((strcmp(argv[1], "--timeout") == 0) || (strcmp(argv[1], "-t")))
					&& (argc > 1)) {
				if (sscanf(argv[1], "%d", &timeout) < 0) {
					fprintf(stderr, "error: bad timeout value\n");
					exit(-1);
				}
			} else {
				fprintf(stderr, "error: unknown op (%s)\n", argv[1]);
				exit(ERROR_DIAL_CALL);
			}
			argi = 2;
		} else {
			op = "execute";
			argi = 1;
		}
	}
	if (argi >= argc) {
		print_usage(argv);
		exit(-1);
	}

	addr = argv[argi];
	argi++;

	if ((conn = russ_dialv(addr, op, timeout, NULL, argc-argi, &argv[argi])) == NULL) {
		fprintf(stderr, "error: cannot dial service\n");
		exit(-1);
	}

	if ((spawn_forward_bytes(&in_th, STDIN_FILENO, conn->fds[0], conn->fds[0]) < 0)
		|| (spawn_forward_bytes(&out_th, conn->fds[1], STDOUT_FILENO, conn->fds[1]) < 0)
		|| (spawn_forward_bytes(&err_th, conn->fds[2], STDERR_FILENO, conn->fds[2]) < 0)) {
		fprintf(stderr, "error: failed to start thread for forwarder\n");
		exit(-1);
	}

	dprintf("waiting...\n");
	pthread_join(out_th, NULL);
	pthread_join(err_th, NULL);
	/* don't join in_th because a closed out and err mean the end */
	/*pthread_join(in_th, NULL);*/

	russ_close_conn(conn);
	conn = russ_free_conn(conn);

	exit(0);
}
