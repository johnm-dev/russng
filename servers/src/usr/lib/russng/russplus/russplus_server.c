/*
** lib/russng/russplus_server.c
*/

/*
# license--start
#
# Copyright 2012-2019 John Marshall
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

#include <dirent.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#include <russ/russ.h>

#define PLUS_COUNT_MAX	16

int
cmpstrp(const void *p0, const void *p1) {
	return strcmp(*(char * const *)p0, *(char * const *)p1);
}

int
print_dir_list(struct russ_sconn *sconn, char *dirpath) {
	struct stat		st;
	struct dirent		*dent = NULL;
	DIR			*dir = NULL;
	char			**names;
	char			path[RUSS_REQ_SPATH_MAX];
	int			errfd, outfd;
	int			cap, i, n;
	int			dtypeunknown;

	outfd = sconn->fds[1];
	errfd = sconn->fds[2];

	if ((dir = opendir(dirpath)) == NULL) {
		russ_dprintf(errfd, "error: cannot open directory\n");
		return -1;
	} else {
		cap = 2048;
		n = 0;
		names = malloc(sizeof(char *)*cap);

		while ((dent = readdir(dir)) != NULL) {
			if ((strcmp(dent->d_name, ".") == 0)
				|| (strcmp(dent->d_name, "..") == 0)) {
				continue;
			}
#if defined(AIX)
			dtypeunknown = 1;
#else
			dtypeunknown = (dent->d_type == DT_UNKNOWN) ? 1 : 0;
#endif
			if (dtypeunknown) {
				if ((russ_snprintf(path, sizeof(path), "%s/%s", dirpath, dent->d_name) < 0)
					|| (lstat(path, &st) < 0)) {
					/* problem */
					continue;
				}
			}

			/* removed check for russ_is_conffile(path) */
			if ((!dtypeunknown && ((dent->d_type == DT_REG) || (dent->d_type == DT_LNK) || (dent->d_type == DT_SOCK)))
				|| (S_ISDIR(st.st_mode) || S_ISSOCK(st.st_mode) || S_ISLNK(st.st_mode))) {
				if ((n == cap) || ((names[n++] = strdup(dent->d_name)) == NULL)) {
					russ_dprintf(errfd, "error: too many entries\n");
					cap = -1;
					closedir(dir);
					goto free_names;
				}
			}
		}
		closedir(dir);

		/* sort and output */
		qsort(names, n, sizeof(char *), cmpstrp);
		for (i = 0; i < n; i++) {
			russ_dprintf(outfd, "%s\n", names[i]);
		}
	}
free_names:
	for (n--; n >= 0; n--) {
		names[n] = russ_free(names[n]);
	}
	names = russ_free(names);
	if (cap < 0) {
		return -1;
	}
	return 0;
}

char *
get_userhome(char *username) {
	struct passwd	*pw = NULL;
	uid_t		uid;
	char		*home = NULL;

	if (username) {
		if ((pw = getpwnam(username)) == NULL) {
			return NULL;
		}
	} else {
		if ((pw = getpwuid(getuid())) == NULL) {
			return NULL;
		}
	}
	if ((home = strdup(pw->pw_dir)) == NULL) {
		return NULL;
	}
	return home;
}

/**
* Resolve plus path.
*
* Path starting with "/" is treated as a path. Path starting with ":"
* is treated as symbolic name which points to one or more specific,
* predefined paths.
*
* @param userhome	path to user home
* @param path		path to resolve
* @return		sarray0 object (caller free); NULL on failure
*/
char **
resolve_plus_path(char *userhome, char *path) {
	char	**paths;
	char	pathbuf[PATH_MAX];

	if (path == NULL) {
		return NULL;
	} else if (path[0] == '/') {
		return russ_sarray0_new(1, path, NULL);
	} else if (path[0] == ':') {
		if (strcmp(path, ":override") == 0) {
			if (russ_snprintf(pathbuf, sizeof(pathbuf), "%s/.russ/bb/override/services", userhome) > 0) {
				return russ_sarray0_new(1, pathbuf, NULL);
			}
		} else if (strcmp(path, ":fallback") == 0) {
			if (russ_snprintf(pathbuf, sizeof(pathbuf), "%s/.russ/bb/fallback/services", userhome) > 0) {
				return russ_sarray0_new(1, pathbuf, NULL);
			}
		} else if (strcmp(path, ":system") == 0) {
			return russ_sarray0_new(1, russ_get_services_dir(), NULL);
		}
	}
	return NULL;
}

/**
* Get list of paths to search for "+" entries.
*
* The paths are given in ~/.russ/plus/bb.paths, one per line. Lines
* starting with "/" are treated as paths. Lines starting with ":"
* are treated as symbolic names which point to specific, predefined
* paths.
*
* If ~/.russ/plus/bb.paths is not found, a default set of paths is
* used:
*   :override
*   :system
*   :builtin
*   :fallback
*
* @return		array of strings; NULL on error
*/
char **
get_plus_paths(void) {
	FILE	*f = NULL;
	char	pathbuf[PATH_MAX];
	char	**paths = NULL, *userhome = NULL;
	int	i;

	if (((userhome = get_userhome(NULL)) == NULL)
		|| (russ_snprintf(pathbuf, sizeof(pathbuf)-1, "%s/.russ/plus/bb.paths", userhome) < 0)
		|| ((paths = russ_sarray0_new(32, NULL)) == NULL)) {
		goto fail;
	}

	if ((f = fopen(pathbuf, "r")) == NULL) {
		if ((russ_sarray0_extend(&paths, resolve_plus_path(userhome, ":override"), 1) < 0)
			|| (russ_sarray0_extend(&paths, resolve_plus_path(userhome, ":system"), 1) < 0)
			|| (russ_sarray0_extend(&paths, resolve_plus_path(userhome, ":builtin"), 1) < 0)
			|| (russ_sarray0_extend(&paths, resolve_plus_path(userhome, ":fallback"), 1) < 0)) {
			goto fail;
		}
	} else {
		for (i = 0; i < 128; i++) {
			if ((fscanf(f, "%128s", pathbuf) < 0)
				|| (strcmp(pathbuf, "") == 0)) {
				break;
			}
			if (strcmp(pathbuf, ":clear") == 0) {
				/* reset list */
				paths[0] = NULL;
			} else if (strcmp(pathbuf, ":default") == 0) {
				if ((russ_sarray0_extend(&paths, resolve_plus_path(userhome, ":override"), 1) < 0)
					|| (russ_sarray0_extend(&paths, resolve_plus_path(userhome, ":system"), 1) < 0)
					|| (russ_sarray0_extend(&paths, resolve_plus_path(userhome, ":builtin"), 1) < 0)
					|| (russ_sarray0_extend(&paths, resolve_plus_path(userhome, ":fallback"), 1) < 0)) {
					goto fail;
				}
			} else if (russ_sarray0_extend(&paths, resolve_plus_path(userhome, pathbuf), 1) < 0) {
				goto fail;
			}
		}

		fclose(f);
	}

	userhome = russ_free(userhome);
	return paths;

fail:
	fclose(f);
	paths = russ_sarray0_free(paths);
	userhome = russ_free(userhome);
	return NULL;
}

/* global */
struct russ_conf	*conf = NULL;
const char		*HELP = 
"Plus server handling \"+\" component in the service path. Passes\n"
"the request to another service with splicing of fds. Alternate\n"
"locations to search are specified in ~/.russ/plus/bb.paths.\n"
"\n"
"/<spath> <args>\n"
"    Dial service at <spath>.\n";


/**
* Answer and service request only if it is for "/".
*
* Listable items are: filenames for socket files, conffiles, and
* results from "list" call to a server.
*/
void
svc_root_handler(struct russ_sess *sess) {
	struct russ_svr		*svr = NULL;
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	struct stat		st;
	char			spath[RUSS_REQ_SPATH_MAX];
	char			*searchpath = NULL, **searchpaths = NULL;
	int			i;

	svr = sess->svr;
	sconn = sess->sconn;
	req = sess->req;

	/* load search paths */
	if ((searchpaths = get_plus_paths()) == NULL) {
		russ_sconn_exit(sconn, RUSS_EXIT_FAILURE);
		russ_sconn_close(sconn);
		exit(0);
	}

	for (i = 0, searchpath = searchpaths[0]; searchpath != NULL; i++, searchpath = searchpaths[i]) {
		russ_snprintf(spath, sizeof(spath), "%s/%s", searchpath, &req->spath[1]);
		if ((stat(spath, &st) == 0) && (!S_ISSOCK(st.st_mode)) && (!russ_is_conffile(spath))) {
			/* non-socket, non-conffile file or directory */
			if (S_ISDIR(st.st_mode)) {
				print_dir_list(sconn, spath);
			}
		} else {
			/* get from server */
			struct russ_cconn	*cconn = NULL;
			char			buf[1<<20];
			int			nread;

			if ((cconn = russ_dialv(russ_to_deadline(10000), "list", spath, NULL, NULL)) != NULL) {
				while (1) {
					nread = russ_readn_deadline(russ_to_deadline(10000), cconn->fds[1], buf, sizeof(buf));
					if (nread <= 0) {
						break;
					}
					russ_writen(sconn->fds[1], buf, nread);
					/* TODO: how to cleanly handle partial reads? or reads not ending in newline? */
				}
				russ_cconn_close(cconn);
				cconn = russ_cconn_free(cconn);
			}
		}
	}
	russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
	russ_sconn_close(sconn);
	searchpaths = russ_sarray0_free(searchpaths);
	exit(0);
}

/**
* Answer and service request at /... and below.
*
* For a request spath/<first>/..., <first> must be a socket file or
* a conffile. Directories are *not* supported. This limitation makes
* certain things simple. Servicing the request means passing the
* request on with redial and splice operations.
*/
void
svc_subroot_handler(struct russ_sess *sess) {
	struct russ_svr		*svr = NULL;
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	struct stat		st;
	char			spath[RUSS_REQ_SPATH_MAX];
	char			*searchpath = NULL, **searchpaths = NULL, *first = NULL;
	char			pluscountbuf[32];
	int			i, pluscount, pluscountidx;

	svr = sess->svr;
	sconn = sess->sconn;
	req = sess->req;

	/*
	* manage plus server loop countprevent plus server loop limit:
	* * use attrv entry __PLUS_COUNT; (re)position at index 0
	* * increment each pass through plus server (only)
	* * limit to PLUS_COUNT_MAX
	* * return error if exceeded
	*/
	if (req->attrv == NULL) {
		req->attrv = russ_sarray0_new(1, NULL);
	}
	if ((pluscountidx = russ_sarray0_find_prefix(req->attrv, "__PLUS_COUNT=")) >= 0) {
		pluscount = atoi((req->attrv[pluscountidx])+13);
	} else {
		pluscount = 0;
	}
	pluscount++;
	if ((pluscount > PLUS_COUNT_MAX)
		|| (russ_snprintf(pluscountbuf, sizeof(pluscountbuf), "__PLUS_COUNT=%d", pluscount) < 0)) {
		if ((svr->answerhandler == NULL) || (svr->answerhandler(sconn) < 0)) {
			/* fatal */
			return;
		}
		russ_sconn_fatal(sconn, "plus server loop limit problem", RUSS_EXIT_FAILURE);
		exit(0);
	}
	if (pluscountidx != 0) {
		russ_sarray0_insert(&req->attrv, 0, pluscountbuf);
	} else {
		russ_sarray0_replace(req->attrv, pluscountidx, pluscountbuf);
	}

	/* load search paths and first component */
	if ((searchpaths = get_plus_paths()) == NULL) {
		goto fail;
	}
	first = russ_str_dup_comp(req->spath, '/', 1);

	/* hand off to first found */
	for (i = 0, searchpath = searchpaths[0]; searchpath != NULL; i++, searchpath = searchpaths[i]) {
		//if ((stat(searchpath, &st) == 0) && (S_ISSOCK(st.st_mode)) || (russ_is_conffile(spath)) {
		if ((stat(searchpath, &st) == 0) && (!S_ISDIR(st.st_mode))) {
			/*
			* TODO: should there be any special handling if searchpath points to
			* a server? This would allow for a super or mredir server to be used
			* there. If so, then it would require a "list" call to check if there
			* is a match.
			*/
			/* skip for now */
			continue;
		}
		if (russ_snprintf(spath, sizeof(spath), "%s/%s", searchpath, first) < 0) {
			goto fail;
		}
		if ((stat(spath, &st) == 0) && (S_ISSOCK(st.st_mode)) || (russ_is_conffile(spath))) {
			russ_snprintf(spath, sizeof(spath), "%s/%s", searchpath, &req->spath[1]);
			req->spath = russ_free(req->spath);
			req->spath = strdup(spath);
			if (russ_sconn_redialandsplice(sconn, RUSS_DEADLINE_NEVER, req) < 0) {
				exit(0);
			}
			goto fail;
		}
	}

fail:
	searchpaths = russ_free(searchpaths);
	if ((svr->answerhandler == NULL) || (svr->answerhandler(sconn) < 0)) {
		/* fatal */
		return;
	}
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: russplus_server [<conf options>]\n"
"\n"
"Searches for services under multiple locations, in order:\n"
"1) ~/.russ/services 2) /run/russ/services. Then, dials and passes\n"
"fds of the dialed service back to the original client.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_svcnode	*root = NULL, *node = NULL;
	struct russ_svr		*svr = NULL;

	signal(SIGPIPE, SIG_IGN);

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_load(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (((svr = russ_init(conf)) == NULL)
		|| (russ_svr_set_type(svr, RUSS_SVR_TYPE_FORK) < 0)
		|| (russ_svr_set_autoswitchuser(svr, 1) < 0)
		|| (russ_svr_set_matchclientuser(svr, 1) < 0)
		|| (russ_svr_set_help(svr, HELP) < 0)

		|| (russ_svcnode_set_handler(svr->root, svc_root_handler) < 0)
		|| (russ_svcnode_set_autoanswer(svr->root, 1) < 0)
		|| (russ_svcnode_set_virtual(svr->root, 0) < 0)
		|| ((node = russ_svcnode_add(svr->root, "*", svc_subroot_handler)) == NULL)
		|| (russ_svcnode_set_autoanswer(node, 0) < 0)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)) {

		fprintf(stderr, "error: cannot set up server\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);
}
