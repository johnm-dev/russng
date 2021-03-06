/*
* lib/req.c
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

#include "russ/priv.h"

/**
* Create new request.
*
* Note: All provided (non NULL) information is duplicated.
*
* @param protocolstring	russ protocol identification string
* @param op		operation string
* @param spath		service path
* @param attrv		NULL-terminated array of attributes ("name=value" strings)
* @param argv		NULL-terminated array of arguments
* @return		request object on success; NULL on failure
*/
struct russ_req *
russ_req_new(const char *protocolstring, const char *op, const char *spath, char **attrv, char **argv) {
	struct russ_req	*self = NULL;
	int		i;

	if ((self = russ_malloc(sizeof(struct russ_req))) == NULL) {
		return NULL;
	}
	self->protocolstring = NULL;
	self->spath = NULL;
	self->op = NULL;
	self->opnum = RUSS_OPNUM_NOTSET;;
	self->attrv = NULL;
	self->argv = NULL;

	if (((protocolstring) && ((self->protocolstring = strdup(protocolstring)) == NULL))
		|| ((op) && ((self->op = strdup(op)) == NULL))
		|| ((spath) && ((self->spath = strdup(spath)) == NULL))) {
		goto free_request;
	}
	self->opnum = russ_optable_find_opnum(NULL, op);
	if (attrv) {
		if ((self->attrv = russ_sarray0_dup(attrv, RUSS_REQ_ATTRS_MAX)) == NULL) {
			goto free_request;
		}
	}
	if (argv) {
		if ((self->argv = russ_sarray0_dup(argv, RUSS_REQ_ARGS_MAX)) == NULL) {
			goto free_request;
		}
	}
	return self;

free_request:
	russ_req_free(self);
	return NULL;
}

/**
* Free request object.
*
* @param self		request object
* @return		NULL
*/
struct russ_req *
russ_req_free(struct russ_req *self) {
	if (self) {
		/* own copy */
		self->protocolstring = russ_free(self->protocolstring);
		self->op = russ_free(self->op);
		self->spath = russ_free(self->spath);
		self->attrv = russ_sarray0_free(self->attrv);
		self->argv = russ_sarray0_free(self->argv);
	}
	return NULL;
}
