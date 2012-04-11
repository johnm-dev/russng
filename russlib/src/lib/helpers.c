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

struct russ_conn *russ_execv(char *saddr, int timeout, char **attrv, int argc, char **argv) {
	return russ_dialv(saddr, "execute", timeout, attrv, argc, argv);
}

char **
russ_variadic_to_argv(int *argc, va_list ap, va_list ap2) {
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

struct russ_conn *russ_execl(char *saddr, int timeout, char **attrv, ...) {
	struct russ_conn	*conn;
	va_list			ap;
	char			**argv;
	int			argc;

	va_start(ap, attrv);
	if ((argv = russ_variadic_to_argv(&argc, ap, ap)) == NULL) {
		return NULL;
	}
	conn = russ_dialv(saddr, "execute", timeout, attrv, argc, argv);
	free(argv);

	return conn;
}

struct russ_conn *russ_help(char *saddr, int timeout) {
	return russ_dialv(saddr, "help", timeout, NULL, 0, NULL);
}

struct russ_conn *russ_info(char *saddr, int timeout) {
	return russ_dialv(saddr, "info", timeout, NULL, 0, NULL);
}

struct russ_conn *russ_list(char *saddr, int timeout) {
	return russ_dialv(saddr, "list", timeout, NULL, 0, NULL);
}
