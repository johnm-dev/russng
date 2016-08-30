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

#include "russ_priv.h"

/**
* Create new service node object.
*
* @param name		node name
* @param handler	service handler
* @return		service node object; NULL on failure
*/
struct russ_svcnode *
russ_svcnode_new(const char *name, russ_svchandler handler) {
	struct russ_svcnode	*self;

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
* @param self		service node object
* @param name		service name
* @param handler	service handler
* @return		child service node object; NULL on failure
*/
struct russ_svcnode *
russ_svcnode_add(struct russ_svcnode *self, const char *name, russ_svchandler handler) {
	struct russ_svcnode	*curr, *last, *node;
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
* A wildcard svcnode always matches.
*
* @param self		service node object starting point
* @param path		path to match (relative to self)
* @param mpath		path matched
* @param mpath_cap	size of mpath buffer
* @return		matching service node object; NULL on failure
*/
struct russ_svcnode *
russ_svcnode_find(struct russ_svcnode *self, const char *path, char *mpath, int mpath_cap) {
	struct russ_svcnode	*node;
	char			*sep;
	int			len, cmp;

	if (self == NULL) {
		return NULL;
	}
	if ((self->virtual) || (strcmp(path, "") == 0)) {
		return self;
	}
	if ((sep = strchr(path, '/')) == NULL) {
		sep = strchr(path, '\0');
	}
	len = sep-path;
	for (node = self->children; node != NULL; node = node->next) {
		if ((!node->wildcard) && ((cmp = strncmp(node->name, path, len)) > 0)) {
			if (mpath != NULL) {
				mpath[0] = '\0';
			}
			node = NULL;
			break;
		} else if (node->wildcard || ((cmp == 0) && (node->name[len] == '\0'))) {
			if (*sep != '\0') {
				if (mpath != NULL) {
					if ((strncat(mpath, "/", mpath_cap) < 0)
						|| (strncat(mpath, node->name, mpath_cap) < 0)) {
						/* do not exceed mpath buffer */
						mpath[0] = '\0';
						node = NULL;
						break;
					}
				}
				node = russ_svcnode_find(node, &path[len+1], mpath, mpath_cap);
			}
			break;
		}
	}
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
