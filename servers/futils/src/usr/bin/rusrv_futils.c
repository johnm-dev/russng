/*
** bin/rusrv_futils.c
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

#ifdef OPENSSL
#include <openssl/md5.h>
#endif

#include <russ.h>

struct russ_conf	*conf = NULL;
char			*default_argv[] = {"-", NULL};
char			*HELP =
"A collection of file utilities.\n"
"\n"
"/head [<options>] [<file> [...]]\n"
"    Output the first part of files.\n"
"\n"
"    Options:\n"
"    -c [-]n        print the first n bytes; with leading '-' print\n"
"                   all but the last n bytes"
"    -n [-]n        print the first n lines; with leading '-' print\n"
"                   all but the last n lines\n"
"\n"
"/md5sum [<file> [...]]\n"
"    Print MD5 (128-bit) checksum.\n"
"\n"
"/read [<options>] [<file>]\n"
"    Output the contents of file. Optionally, a range may be\n"
"    specified from <start> upto, but not including, <end>.\n"
"    Ranges start at index 0.\n"
"\n"
"    Options:\n"
"    -c <start>[:<end>]\n"
"                   print bytes from the range <start> upto <end>-1\n"
"    -n <start>[:<end>]\n"
"                   print lines from the range <start> upto <end>-1\n"
"\n"
"/tail\n"
"    Output the last part of files.\n"
"\n"
"    Options:\n"
"    -c [+]n        print the last n bytes; with leading '+' print\n"
"                   starting with nth byte\n"
"    -n [+]n        print the last n lines; with leading '+' print\n"
"                   all but the last n lines\n"
"\n"
"/wc [<file> [...]]\n"
"    Print newline, word, and byte counts for each file. A word is a\n"
"    non-zero-length sequence of characters delimited by white space.\n"
"\n"
"    -c             print the byte counts\n"
"    -m             print the character counts\n"
"    -l             print the newline counts\n"
"    -w             print the word counts\n"
"\n"
"/write [-s <str>] <file>\n"
"    Write to a file. If -s is specified, then <str> will be written\n"
"    to the named file. Otherwise, input will be read from stdin.\n"
"\n"
"In all cases except write, if <file> is not given or is '-', input\n"
"will be read from stdin.\n";

/**
* Parse argument list for -c and -n options. Used by head and tail
* for byte and line settings
*
* @param argv		argument list
* @param copt		1 if specified; 0 by default
* @param cval		long int value if -c specified
* @param nopt		2 if +<nval>; 1 if specified but not
*			+<nval>; 0 by default
* @param nval		long in value if -n specified
* @return		pointer to next argument in argument list
*/
char **
parse_copt_nopt(char **argv, int *copt, long *cval, int *nopt, long *nval) {
	if (argv) {
		for (; *argv; argv++) {
			if (strcmp(*argv, "-c") == 0) {
				if ((*(++argv) == NULL)
					|| (sscanf(*argv, "%ld", cval) < 0)) {
					return NULL;
				}
				if (*argv[0] == '+') {
					*copt = 2;
				} else {
					*copt = 1;
				}
			} else if (strcmp(*argv, "-n") == 0) {
				if ((*(++argv) == NULL)
					|| (sscanf(*argv, "%ld", nval) < 0)) {
					return NULL;
				}
				if (*argv[0] == '+') {
					*nopt = 2;
				} else {
					*nopt = 1;
				}
			} else {
				break;
			}
		}
	}
	return argv;
}

/**
* Find nth byte in file stream starting at the beginning.
*
* @param f		file object
* @param ch		byte to match
* @param cnt		# of ch to match
* @return		index in file of nth match
*/
long
find_nth(FILE *f, char ch, long cnt) {
	char	buf[4096];
	int	i, n;

	fseek(f, 0, SEEK_SET);
	while (cnt > 0) {
		if ((n = fread(buf, 1, sizeof(buf), f)) < 0) {
			break;
		}
		for (i = 0; i < n; i++) {
			if (buf[i] == ch) {
				cnt--;
				if (cnt == 0) {
					fseek(f, -(n-i), SEEK_CUR);
					goto done;
				}
			}
		}
	}
done:
	return ftell(f);
}

/**
* Find nth byte in file file starting at the end.
*
* @param f		file object
* @param ch		byte to match
* @param cnt		# of ch to match
* @param ignore_last	non-zero if last ch matches
* @return		index in file of nth match (from end)
*/
long
find_nth_rev(FILE *f, char ch, long cnt, int ignore_last) {
	char	buf[4096];
	long	lastpos;
	int	i, n;

	if (ignore_last) {
		fseek(f, -1, SEEK_END);
	} else {
		fseek(f, 0, SEEK_END);
	}
	lastpos = ftell(f);
	while ((cnt > 0) && (lastpos > 0)) {
fprintf(stderr, "cnt (%ld) lastpos (%ld)\n", cnt, lastpos);
		if (lastpos < sizeof(buf)) {
			/* short read */
			fseek(f, 0, SEEK_SET);
			n = fread(buf, 1, lastpos, f);
		} else {
			fseek(f, lastpos-sizeof(buf), SEEK_SET);
			n = fread(buf, 1, sizeof(buf), f);
		}
		if (n < 0) {
			return -1;
		}
		for (i = n-1; i >= 0; i--) {
			if ((buf[i] == ch) && (--cnt == 0)) {
				lastpos -= (n-i);
				goto done;
			}
		}
		lastpos -= n;
	}
done:
	return lastpos;
}

/**
* Copy range of bytes from file object to fd. Optionally add a
* newline.
*
* @param fd		descriptor to write to
* @param f		file object to read from
* @param spos		starting byte position
* @param epos		ending byte position (not included)
* @param addnl		non-zero to add newline
* @return		0 on success; -1 on failure
*/
int
print_range(int fd, FILE *f, long spos, long epos, int addnl) {
	char	buf[4096+1];
	int	n;

	buf[sizeof(buf)-1] = '\0';
	fseek(f, spos, SEEK_SET);
	while (spos < epos) {
		if ((n = fread(buf, 1, sizeof(buf)-1, f)) < 0) {
			return -1;
		}
		if (n == 0) {
			break;
		}
		if (spos+n > epos) {
			n = epos-spos;
		}
		if (n < sizeof(buf)) {
			buf[n] = '\0';
		}
		russ_dprintf(fd, "%s", buf);
		spos += n;
	}
	/* add newline if last char was not newline */
	if (addnl && (n != 0) && (buf[n-1] != '\n')) {
		russ_dprintf(fd, "\n");
	}
	return 0;
}

/**
* Copy lines from file to fd from current position.
*
* @param fd		descriptor to write to
* @param f		file object to read from
* @param cnt		# of lines to copy
* @return		0 on success; -1 on failure
*/
int
print_next_lines(int fd, FILE *f, int cnt) {
	char	buf[4096+1];
	int	i, n;

	i = 0;
	buf[sizeof(buf)-1] = '\0';
	while (cnt > 0) {
		if ((n = fread(buf, 1, sizeof(buf)-1, f)) < 0) {
			return -1;
		}
		if (n == 0) {
			break;
		}
		for (i = 0; i < n; i++) {
			if ((buf[i] == '\n') && (--cnt == 0)) {
				buf[i+1] = '\0';
				break;
			}
		}
		russ_dprintf(fd, "%s", buf);
	}
	return 0;
}

void
svc_root_handler(struct russ_sess *sess) {
	/* auto handling in svr */
}

/**
* Print the "head" of a file a-la the head program.
*
* @param sess		session object
*/
void
svc_head_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	FILE			*f;
	char			**argv;
	char			*filename, **filenames;
	int			copt = 0, nopt = 1;
	long			cval = 0, nval = -10;
	int			i, cnt, n;

	switch (req->opnum) {
	case RUSS_OPNUM_EXECUTE:
		argv = req->argv;
		if ((argv) && ((argv = parse_copt_nopt(argv, &copt, &cval, &nopt, &nval)) == NULL)) {
			/* parse error */
			russ_sconn_fatal(sconn, RUSS_MSG_BAD_ARGS, RUSS_EXIT_FAILURE);
		}
		filenames = ((argv == NULL) || (argv[0] == NULL)) \
			? (char **)default_argv : req->argv;
		for (filename = *filenames; *filenames; filename = *(++filenames)) {
			if ((f = fopen(filename, "r")) == NULL) {
				continue;
			}
			cnt = 0;
			if (copt) {
				if (cval < 0) {
					/* relative to end */
					fseek(f, cval, SEEK_END);
					cval = ftell(f);
				}
				print_range(sconn->fds[1], f, 0, cval, 0);
			} else {
				if (nval < 0) {
					/* find pos of last newline */
					cval = find_nth_rev(f, '\n', RUSS__ABS(nval), 1);
					print_range(sconn->fds[1], f, 0, cval, 1);
				} else {

					print_next_lines(sconn->fds[1], f, nval);
				}
			}
			fclose(f);
		}
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		break;
	default:
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
	exit(0);
}

#ifdef OPENSSL
/**
* Calculate and print md5 checksum of one or more files.
*
* @param sess		session object
*/
void
svc_md5sum_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	FILE			*f;
	MD5_CTX			mdctxt;
	unsigned char		dig[MD5_DIGEST_LENGTH];
	char			buf[4096];
	char			*filename, **filenames;
	int			i, n;

	switch (req->opnum) {
	case RUSS_OPNUM_EXECUTE:
		filenames = ((req->argv == NULL) || (req->argv[0] == NULL)) \
			? (char **)default_argv \
			: req->argv;
		for (filename = *filenames; (filename != NULL); filename = *(++filenames)) {
			if (strcmp(filename, "-") == 0) {
				if ((f = fdopen(sconn->fds[0], "r")) == NULL) {
					continue;
				}
			} else {
				if ((f = fopen(filename, "r")) == NULL) {
					continue;
				}
			}
			MD5_Init(&mdctxt);
			while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
				MD5_Update(&mdctxt, buf, n);
			}
			if (!feof(f)) {
				/* error */
			}
			fclose(f);
			MD5_Final(dig, &mdctxt);
			for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
				russ_dprintf(sconn->fds[1], "%02x", dig[i]);
			}
			russ_dprintf(sconn->fds[1], " %s\n", filename);
		}
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		break;
	default:
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
	exit(0);
}
#endif /* OPENSSL */

/**
* Read and print contents of file to stdout.
*
* @param sess		session object
*/
void
svc_read_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;

	switch (req->opnum) {
	case RUSS_OPNUM_EXECUTE:
		russ_dprintf(sconn->fds[1], "%s", HELP);
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		break;
	default:
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
	exit(0);
}

/**
* Print "tail" of file a-la the tail program.
*
* @param sess		session object
*/
void
svc_tail_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	FILE			*f;
	char			**argv;
	char			*filename, **filenames;
	int			copt = 0, nopt = 1;
	long			cval = 0, nval = -10;
	int			i, cnt, n;

	switch (req->opnum) {
	case RUSS_OPNUM_EXECUTE:
		argv = req->argv;
		if ((argv) && ((argv = parse_copt_nopt(argv, &copt, &cval, &nopt, &nval)) == NULL)) {
			/* parse error */
			russ_sconn_fatal(sconn, RUSS_MSG_BAD_ARGS, RUSS_EXIT_FAILURE);
		}
		filenames = ((argv == NULL) || (argv[0] == NULL)) \
			? (char **)default_argv \
			: req->argv;
		for (filename = *filenames; *filenames; filename = *(++filenames)) {
			if ((f = fopen(filename, "r")) == NULL) {
				continue;
			}
			cnt = 0;
			if (copt) {
				if (copt == 1) {
					/* relative to end */
					fseek(f, 0L, SEEK_END);
					cval = ftell(f)-RUSS__ABS(cval);
				}
				print_range(sconn->fds[1], f, cval, ftell(f), 0);
			} else {
				if (nopt == 1) {
					/* find pos of last newline */
					cval = find_nth_rev(f, '\n', RUSS__ABS(nval), 1);
					cval = (cval == 0) ? 0 : cval+1;
					fseek(f, 0L, SEEK_END);
					print_range(sconn->fds[1], f, cval, ftell(f), 1);
				} else {
					cval = find_nth(f, '\n', RUSS__MAX(nval-1, 0));
					cval = (cval == 0) ? 0 : cval+1;
					fseek(f, 0L, SEEK_END);
					/* don't add nl */
					print_range(sconn->fds[1], f, cval, ftell(f), 0);
				}
			}
			fclose(f);
		}
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		break;
	default:
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
	exit(0);
}

/**
* Count and print byte, word, and line counts.
*
* @param sess		session object
*/
void
svc_wc_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	FILE			*f;
	char			**argv;
	char			*filename, **filenames;
	int			ch, nwords, nlines;
	long			nbytes, lastwhite;

	switch (req->opnum) {
	case RUSS_OPNUM_EXECUTE:
		argv = req->argv;
		filenames = ((argv == NULL) || (argv[0] == NULL)) \
			? (char **)default_argv \
			: req->argv;
		for (filename = *filenames; *filenames; filename = *(++filenames)) {
			if ((f = fopen(filename, "r")) == NULL) {
				continue;
			}
			nbytes = 0;
			nwords = 0;
			nlines = 0;
			lastwhite = 0;
			for (ch = fgetc(f); ch != EOF; ch = fgetc(f)) {
				nbytes++;
				switch (ch) {
				case ' ':
				case '\t':
					lastwhite = nbytes;
					break;
				case '\n':
					nlines++;
					lastwhite = nbytes;
					break;
				default:
					if (lastwhite == nbytes-1) {
						nwords++;
					}
					break;
				}
			}
			if ((lastwhite != -1) && (lastwhite == nbytes-1)) {
				nwords++;
			}
			russ_dprintf(sconn->fds[1], "%d %d %ld %s\n", nlines, nwords, nbytes, filename);
			fclose(f);
		}
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		break;
	default:
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
	exit(0);
}

/**
* Write bytes from stdin or string argument.
*
* @param sess		session object
*/
void
svc_write_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	int			fd;
	char			**argv;
	char			*filename, *s;

	switch (req->opnum) {
	case RUSS_OPNUM_EXECUTE:
		argv = req->argv;
		if (argv == NULL) {
			russ_sconn_fatal(sconn, RUSS_MSG_BAD_ARGS, RUSS_EXIT_FAILURE);
			goto done;
		}
		filename = NULL;
		s = NULL;
		if (strcmp(*argv, "-s") == 0) {
			argv++;
			if (*argv == NULL) {
				russ_sconn_fatal(sconn, RUSS_MSG_BAD_ARGS, RUSS_EXIT_FAILURE);
				goto done;
			}
			s = *argv;
			argv++;
		}
		if ((*argv == NULL) || (*(argv+1))) {
			russ_sconn_fatal(sconn, RUSS_MSG_BAD_ARGS, RUSS_EXIT_FAILURE);
			goto done;
		}
		filename = *argv;

		/* use fd instead of FILE because we're using russ_write */
		if ((fd = open(filename, O_WRONLY)) < 0) {
			russ_sconn_fatal(sconn, "error: could not open file", RUSS_EXIT_FAILURE);
			goto done;
		}
		if (s != NULL) {
			if (russ_write(fd, s) < 0) {
				russ_sconn_fatal(sconn, "error: could not write to file", RUSS_EXIT_FAILURE);
				goto done;
			}
		} else {
			char	buf[4096];
			int	n;

			while (1) {
				if ((n = russ_read(sconn->fds[0], buf, sizeof(buf))) < 0) {
					russ_sconn_fatal(sconn, "error: could not read input", RUSS_EXIT_FAILURE);
				} else if (n == 0) {
					break;
				}
				if (russ_write(fd, buf, n) < n) {
					russ_sconn_fatal(sconn, "error: could not write to file", RUSS_EXIT_FAILURE);
				}
			}
		}
		break;
	default:
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
done:
	if (fd) {
		close(fd);
	}
	exit(0);
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_echo [<conf options>]\n"
"\n"
"Russ-based file utilities server.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_svcnode	*root;
	struct russ_svr		*svr;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (((root = russ_svcnode_new("", svc_root_handler)) == NULL)
		|| (russ_svcnode_add(root, "head", svc_head_handler) == NULL)
#ifdef OPENSSL
		|| (russ_svcnode_add(root, "md5sum", svc_md5sum_handler) == NULL)
#endif /* OPENSSL */
		|| (russ_svcnode_add(root, "read", svc_read_handler) == NULL)
		|| (russ_svcnode_add(root, "tail", svc_tail_handler) == NULL)
		|| (russ_svcnode_add(root, "wc", svc_wc_handler) == NULL)
		|| (russ_svcnode_add(root, "write", svc_write_handler) == NULL)
		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK)) == NULL)
		|| (russ_svr_set_auto_switch_user(svr, 1) < 0)
		|| (russ_svr_set_help(svr, HELP) < 0)) {
		fprintf(stderr, "error: cannot set up\n");
		exit(1);
	}

	if (russ_svr_announce(svr,
		russ_conf_get(conf, "server", "path", NULL),
		russ_conf_getsint(conf, "server", "mode", 0666),
		russ_conf_getint(conf, "server", "uid", getuid()),
		russ_conf_getint(conf, "server", "gid", getgid())) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);
}
