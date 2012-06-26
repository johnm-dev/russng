/*
** lib/helpers.c
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
russ_execv(russ_timeout timeout, char *addr, char **attrv, char **argv) {
	return russ_dialv(timeout, "execute", addr, attrv, argv);
}

/**
* Wrapper for russ_dial with "execute" operation.
*
* @see russ_diall()
*/
struct russ_conn *
russ_execl(russ_timeout timeout, char *addr, char **attrv, ...) {
	struct russ_conn	*conn;
	va_list			ap;
	char			**argv;
	int			argc;

	va_start(ap, attrv);
	if ((argv = __russ_variadic_to_argv(&argc, ap, ap)) == NULL) {
		return NULL;
	}
	conn = russ_dialv(timeout, "execute", addr, attrv, argv);
	free(argv);

	return conn;
}

/**
* Wrapper for russ_dial with "help" operation.
*
* @param timeout	time allowed to complete operation
* @param addr		full service address
* @return		connection object
*/
struct russ_conn *
russ_help(russ_timeout timeout, char *addr) {
	return russ_dialv(timeout, "help", addr, NULL, NULL);
}

/**
* Wrapper for russ_dial with "info" operation.
*
* @param timeout	time allowed to complete operation
* @param addr		full service address
* @return		connection object
*/
struct russ_conn *
russ_info(russ_timeout timeout, char *addr) {
	return russ_dialv(timeout, "info", addr, NULL, NULL);
}

/**
* Wrapper for russ_dial with "list" operation.
*
* @param timeout	time allowed to complete operation
* @param addr		full service address
* @return		connection object
*/
struct russ_conn *
russ_list(russ_timeout timeout, char *addr) {
	return russ_dialv(timeout, "list", addr, NULL, NULL);
}
