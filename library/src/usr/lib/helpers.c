/*
** lib/helpers.c
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

#include <stdarg.h>
#include <stdlib.h>

#include "russ.h"

/*
* Convert variadic argument list of "char *" to argv array.
*
* @param[out] argc	size of argv array
* @param ap		va_list for counting args
* @param ap2		va_list for creating argv array
* @return		argv array
*/
static char **
__russ_variadic_to_argv(int *argc, va_list ap, va_list ap2) {
	char	**argv;
	int	i;

	/* count args */
	for (i = 0; va_arg(ap, char *) != NULL ; i++);
	va_end(ap);

	/* create argv */
	if ((argv = malloc(sizeof(char *)*i)) == NULL) {
		return NULL;
	}
	*argc = i;
	for (i = 0; i < *argc; i++) {
		argv[i] = va_arg(ap2, char *);
	}
	va_end(ap2);

	return argv;
}

/**
* Wrapper for russ_dial with "execute" operation.
*
* @see russ_dialv()
*/
struct russ_conn *
russ_execv(russ_deadline deadline, char *addr, char **attrv, char **argv) {
	return russ_dialv(deadline, "execute", addr, attrv, argv);
}

/**
* Wrapper for russ_dial with "execute" operation.
*
* @see russ_diall()
*/
struct russ_conn *
russ_execl(russ_deadline deadline, char *addr, char **attrv, ...) {
	struct russ_conn	*conn;
	va_list			ap;
	char			**argv;
	int			argc;

	va_start(ap, attrv);
	if ((argv = __russ_variadic_to_argv(&argc, ap, ap)) == NULL) {
		return NULL;
	}
	conn = russ_dialv(deadline, "execute", addr, attrv, argv);
	free(argv);

	return conn;
}

/**
* Wrapper for russ_dial with "help" operation.
*
* @param deadilne	deadline to complete operation
* @param addr		full service address
* @return		connection object
*/
struct russ_conn *
russ_help(russ_deadline deadline, char *addr) {
	return russ_dialv(deadline, "help", addr, NULL, NULL);
}

/**
* Wrapper for russ_dial with "info" operation.
*
* @param deadline	deadline to complete operation
* @param addr		full service address
* @return		connection object
*/
struct russ_conn *
russ_info(russ_deadline deadline, char *addr) {
	return russ_dialv(deadline, "info", addr, NULL, NULL);
}

/**
* Wrapper for russ_dial with "list" operation.
*
* @param deadline	deadline to complete operation
* @param addr		full service address
* @return		connection object
*/
struct russ_conn *
russ_list(russ_deadline deadline, char *addr) {
	return russ_dialv(deadline, "list", addr, NULL, NULL);
}
