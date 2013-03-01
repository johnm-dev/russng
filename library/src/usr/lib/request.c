/*
** lib/request.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "russ_priv.h"

/**
* Initialize connection request.
*
* Note: All provided (non NULL) information is duplicated.
*
* @param self		request object
* @param protocol_string
*			russ protocol identification string
* @param op		operation
* @param spath		service path
* @param attrv		NULL-terminated array of attributes ("name=value" strings)
* @param argv		NULL-terminated array of arguments
* @return		0 on success; -1 on error
*/
int
russ_req_init(struct russ_req *self, char *protocol_string, russ_op op, char *spath, char **attrv, char **argv) {
	int			i;

	self->protocol_string = NULL;
	self->spath = NULL;
	self->op = op;
	self->op_ext = RUSS_OP_NULL;
	self->attrv = NULL;
	self->argv = NULL;

	if (((protocol_string) && ((self->protocol_string = strdup(protocol_string)) == NULL))
		|| ((spath) && ((self->spath = strdup(spath)) == NULL))) {
		goto free_req_items;
	}
	if (attrv) {
		if ((self->attrv = russ_sarray0_dup(attrv, RUSS_REQ_ATTRS_MAX)) == NULL) {
			goto free_req_items;
		}
	}
	if (argv) {
		if ((self->argv = russ_sarray0_dup(argv, RUSS_REQ_ARGS_MAX)) == NULL) {
			goto free_req_items;
		}
	}
	return 0;

free_req_items:
	russ_req_free_members(self);
	return -1;
}

/**
* Free allocated request members.
*
* All allocations done in russ_req_init() are freed and members
* set to NULL.
*
* Note: The request itself is not freed.
*
* @param self		request object
*/
void
russ_req_free_members(struct russ_req *self) {
	int			i;

	free(self->protocol_string);
	free(self->spath);
	if (self->attrv) {
	    for (i = 0; self->attrv[i] != NULL; i++) {
		    free(self->attrv[i]);
	    }
	    free(self->attrv);
	}
	if (self->argv) {
	    for (i = 0; self->argv[i] != NULL; i++) {
		    free(self->argv[i]);
	    }
	    free(self->argv);
	}
	russ_req_init(self, NULL, NULL, NULL, NULL, NULL);
}
