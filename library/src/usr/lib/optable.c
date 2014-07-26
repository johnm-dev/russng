/*
* lib/optable.c
*/

/*
# license--start
#
# Copyright 2013 John Marshall
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

#include "russ_priv.h"

struct russ_optable russ_optable[] = {
	{ "execute", RUSS_OPNUM_EXECUTE },
	{ "list", RUSS_OPNUM_LIST },
	{ "help", RUSS_OPNUM_HELP },
	{ "id", RUSS_OPNUM_ID },
	{ "info", RUSS_OPNUM_INFO },
	{ NULL, RUSS_OPNUM_EXTENSION },
};

/**
* Look up an operation string and return an op value corresponding
* to it.
*
* The standard operation strings are recognized as are integer
* values up to the sizeof(russ_opnum) == sizeof(uint32_t).
*
* @param self		russ_optable object; NULL to use default
* @param str		operation string
* @return		op value; RUSS_OPNUM_EXTENSION if no match
*/
russ_opnum
russ_optable_find_opnum(struct russ_optable *self, const char *str) {
	if (self == NULL) {
		self = russ_optable;
	}

	if (str == NULL) {
		return RUSS_OPNUM_NOTSET;
	}
	/* RUSS_OPNUM_EXTENSION tests last to ensure match */
	for (; ; self++) {
		if ((self->num == RUSS_OPNUM_EXTENSION) || (strcmp(str, self->str) == 0)) {
			return self->num;
		}
	}
}

/**
* Look up an operation value and return an operation string
* corresponding to it.
*
* @param self		russ_optable object; NULL to use default
* @param opnum		opnum value
* @return		operation string (constant); NULL if no match
*/
const char *
russ_optable_find_op(struct russ_optable *self, russ_opnum opnum) {
	if (self == NULL) {
		self = russ_optable;
	}

	for (; self->num != RUSS_OPNUM_NOTSET; self++) {
		if (self->num == opnum) {
			break;
		}
	}
	return self->str;
}
