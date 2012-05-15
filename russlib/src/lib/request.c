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
* Initialize connection request. All provided (non NULL) information is duplicated.
*/
int
russ_request_init(struct russ_request *req, char *protocol_string, char *spath, char *op, char **attrv, char **argv) {
	int			i;

	req->protocol_string = NULL;
	req->spath = NULL;
	req->op = NULL;
	req->attrv = NULL;
	req->argv = NULL;

	if (((protocol_string) && ((req->protocol_string = strdup(protocol_string)) == NULL))
		|| ((spath) && ((req->spath = strdup(spath)) == NULL))
		|| ((op) && ((req->op = strdup(op)) == NULL))) {
		goto free_req_items;
	}
	if (attrv) {
		if ((req->attrv = russ_dup_str_array0(attrv, RUSS_MAX_ATTRC)) == NULL) {
			goto free_req_items;
		}
	}
	if (argv) {
		if ((req->argv = russ_dup_str_array0(argv, RUSS_MAX_ARGC)) == NULL) {
			goto free_req_items;
		}
	}
	return 0;

free_req_items:
	russ_request_free_members(req);
	return -1;
}

void
russ_request_free_members(struct russ_request *req) {
	int			i;

	free(req->protocol_string);
	free(req->spath);
	free(req->op);
	if (req->attrv) {
	    for (i = 0; req->attrv[i] != NULL; i++) {
		    free(req->attrv[i]);
	    }
	    free(req->attrv);
	}
	if (req->argv) {
	    for (i = 0; req->argv[i] != NULL; i++) {
		    free(req->argv[i]);
	    }
	    free(req->argv);
	}
	russ_request_init(req, NULL, NULL, NULL, NULL, NULL);
}

