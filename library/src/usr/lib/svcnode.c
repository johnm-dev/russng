/*
* lib/svcnode.c
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

#include "russ/priv.h"

/**
* Create new service node object.
*
* @param name		node name
* @param handler	service handler
* @return		service node object; NULL on failure
*/
struct russ_svcnode *
russ_svcnode_new(const char *name, russ_svchandler handler) {
	struct russ_svcnode	*self = NULL;

	if ((self = russ_malloc(sizeof(struct russ_svcnode))) == NULL) {
		return NULL;
	}
	if ((self->name = strdup(name)) == NULL) {
		goto free_node;
	}
	self->handler = handler;
	self->next = NULL;
	self->children = NULL;

	self->autoanswer = 1;
	self->virtual = 0;
	self->wildcard = 0;
	return self;
free_node:
	self = russ_free(self);
	return NULL;
}

/**
* Free service node.
*
* @param self		service node object
* @return		NULL
*/
struct russ_svcnode *
russ_svcnode_free(struct russ_svcnode *self) {
	if (self != NULL) {
		self->name = russ_free(self->name);
		self = russ_free(self);
	}
	return NULL;
}

/**
* Add child service node.
*
* Nodes are added in strcmp() order. This allows linear searching and
* short circuit.
*
* @param self		service node object
* @param name		service name
* @param handler	service handler
* @return		child service node object; NULL on failure
*/
struct russ_svcnode *
russ_svcnode_add(struct russ_svcnode *self, const char *name, russ_svchandler handler) {
	struct russ_svcnode	*curr = NULL, *last = NULL, *node = NULL;
	int			cmp;

	if (self == NULL) {
		return NULL;
	}
	last = NULL;
	curr = self->children;
	while (curr != NULL) {
		if ((cmp = strcmp(curr->name, name)) == 0) {
			/* already exists */
			return NULL;
		} else if (cmp > 0) {
			break;
		}
		last = curr;
		curr = curr->next;
	}
	if ((node = russ_svcnode_new(name, handler)) == NULL) {
		return NULL;
	}
	if (last == NULL) {
		node->next = self->children;
		self->children = node;
	} else {
		last->next = node;
		node->next = curr;
	}

	return node;
}

/**
* Find node matching path starting at a node.
*
* spath options are ignored. A wildcard svcnode always matches.
*
* See russ_svcnode_add() for ordering of child nodes.
*
* @param self		service node object starting point
* @param path		path to match (relative to self)
* @param mpath		path matched
* @param mpath_cap	size of mpath buffer
* @return		matching service node object; NULL on failure
*/
struct russ_svcnode *
russ_svcnode_find(struct russ_svcnode *self, const char *path, char *mpath, int mpath_cap) {
	struct russ_svcnode	*node = NULL;
	const char		*nsep = NULL, *qsep = NULL, *ssep = NULL;
	char			*mpathend = NULL;
	int			cmp, nlen, qlen, slen;

	//russ_lprintf("/tmp/svcfind.log", NULL, "*** mpath (%s) mpath_cap (%d)\n", mpath, mpath_cap);
	if (self == NULL) {
		return NULL;
	}
	/* skip leading / */
	if (path[0] == '/') {
		path++;
	}
	if ((self->virtual) || (strcmp(path, "") == 0)) {
		return self;
	}
	if ((ssep = strchr(path, '/')) == NULL) {
		ssep = strchr(path, '\0');
	}

	for (qsep = path; (qsep < ssep) && (*qsep != '?'); qsep++);
	qlen = qsep-path;
	slen = ssep-path;
	nsep = (qsep < ssep) ? qsep : ssep;
	nlen = nsep-path;

	//russ_lprintf("/tmp/svcfind.log", NULL, "name (%s) path (%s) qlen (%d) slen (%d) nlen (%d)\n", self->name, path, qlen, slen, nlen);
	for (node = self->children; node != NULL; node = node->next) {
		/* strcmp() ordering */
		cmp = strncmp(node->name, path, nlen);
		//russ_lprintf("/tmp/svcfind.log", NULL, "node->name (%s) cmp (%d) wildcard (%d) name[nlen] (%c)\n", node->name, cmp, node->wildcard, node->name[nlen]);

		/* matching component name? */
		if ((!node->wildcard) && (cmp > 0)) {
			if (mpath != NULL) {
				mpath[0] = '\0';
			}
			node = NULL;
			break;
		} else if (node->wildcard || ((cmp == 0) && (node->name[nlen] == '\0'))) {
			/* wildcard or full match and matching component and *name* length */
			if (mpath != NULL) {
				//russ_lprintf("/tmp/svcfind.log", NULL, "updating mpath from (%s)\n", mpath);
				mpathend = strchr(mpath, '\0');
				if ((mpathend-mpath+1+slen+1) < mpath_cap) {
					mpathend[0] = '/';
					mpathend++;
					if (strncat(mpathend, path, slen) < 0) {
						/* do not exceed mpath buffer */
						mpath[0] = '\0';
						node = NULL;
						break;
					}
				}
				//russ_lprintf("/tmp/svcfind.log", NULL, "updated mpath to (%s)\n", mpath);
			}
			if (*ssep != '\0') {
				node = russ_svcnode_find(node, &path[slen+1], mpath, mpath_cap);
			}
			break;
		}
	}
	//russ_lprintf("/tmp/svcfind.log", NULL, "node (%p) mpath (%s)\n", node, mpath);
	//if (node) {
		//russ_lprintf("/tmp/svcfind.log", NULL, "node->name (%s)\n", node->name);
	//}
	return node;
}

int
russ_svcnode_set_autoanswer(struct russ_svcnode *self, int value) {
	if (self == NULL) {
		return -1;
	}
	self->autoanswer = value;
	return 0;
}

int
russ_svcnode_set_handler(struct russ_svcnode *self, russ_svchandler handler) {
	if (self == NULL) {
		return -1;
	}
	self->handler = handler;
	return 0;
}

int
russ_svcnode_set_virtual(struct russ_svcnode *self, int value) {
	if (self == NULL) {
		return -1;
	}
	self->virtual = value;
	return 0;
}

int
russ_svcnode_set_wildcard(struct russ_svcnode *self, int value) {
	if (self == NULL) {
		return -1;
	}
	self->wildcard = value;
	return 0;
}
