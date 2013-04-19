/*
** bin/rusrv_sync_master.c
*/

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "russ.h"

struct children {
	pid_t	pid;
	char	*saddr;
};

#define MAX_CHILDREN	128
struct children	children[MAX_CHILDREN];
int		nchildren = 0;

/**
* Request handler.
*
* @param sess		session object
* @return		0 on success, -1 on error
*/
int
req_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;

	if (conn->req->opnum == RUSS_OPNUM_EXECUTE) {
		/* serve the input from fd passed to client */
		char	buf[1024];
		ssize_t	n;

		while ((n = russ_read(conn->fds[0], buf, sizeof(buf))) > 0) {
			russ_writen(conn->fds[1], buf, n);
		}
	} else {
		return -1;
	}
	return 0;
}

void
print_usage(char **argv) {
	char	*prog_name;

	prog_name = basename(argv[0]);
	fprintf(stderr,
"usage: %s <saddr> ...\n"
"\n"
"Russ-based sync server. Sets up and tracks multiple kinds of\n"
"syncronization servers.\n"
"\n"
"Services:\n"
"/barrier [-t <timeout>] <saddr> <mode> <uid> <gid> <count>\n"
"		Set up a barrier for <count> connections at <saddr>.\n"
"/counter [-t <timeout>] <name>\n"
"\tCount connections at <name>. Associated connections are released\n"
"\timmediately.Optionally time out.\n",
		prog_name);
}

time_t
get_next_time(struct barrier *barriers) {
	time_t	next_time;
	int	i;

	next_time = 
	for (i = 0; i < MAX_BARRIERS; i++) {
	}
}

int
main(int argc, char **argv) {
	struct russ_lis	*lis;
	char		*saddr;
	time_t		next_time;

	signal(SIGCHLD, SIG_IGN);

	if (argc != 2) {
		print_usage(argv);
		exit(1);
	}
	saddr = argv[1];

	if ((lis = russ_announce(saddr, 0666, getuid(), getgid())) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(1);
	}

	poll_fds[0].fd = lis->sd;
	poll_fds[0].events = POLLIN|POLLHUP|POLLNVAL;
	poll_nfds = 1;

	while (1) {
		next_time = get_next_time(barriers);
		poll(poll_fds, poll_nfds, 
	}
}
