/*
** lib/request.c
*/

/*
# license--start
#
# This file is part of the RUSS library.
# Copyright (C) 2012 John Marshall
#
# The RUSS library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
* @param op		operation string
* @param spath		service path
* @param attrv		NULL-terminated array of attributes ("name=value" strings)
* @param argv		NULL-terminated array of arguments
* @return		0 on success; -1 on error
*/
int
russ_request_init(struct russ_request *self, char *protocol_string, char *op, char *spath, char **attrv, char **argv) {
	int			i;

	self->protocol_string = NULL;
	self->spath = NULL;
	self->op = NULL;
	self->attrv = NULL;
	self->argv = NULL;

	if (((protocol_string) && ((self->protocol_string = strdup(protocol_string)) == NULL))
		|| ((op) && ((self->op = strdup(op)) == NULL))
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
	russ_request_free_members(self);
	return -1;
}

/**
* Free allocated request members.
*
* All allocations done in russ_request_init() are freed and members
* set to NULL.
*
* Note: The request itself is not freed.
*
* @param self		request object
*/
void
russ_request_free_members(struct russ_request *self) {
	int			i;

	free(self->protocol_string);
	free(self->op);
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
	russ_request_init(self, NULL, NULL, NULL, NULL, NULL);
}
