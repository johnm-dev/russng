/*
* lib/args.c
*/

/*
# license--start
#
# Copyright 2018 John Marshall
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

#include <russ/priv.h>

/*
* Create NULL-terminated argv array from variadic argument list of
* "char *". The argv array does not allocate new memory for the
* individual items, but points to the strings in the va_list. To
* provide flexibility, an initial size of argv is specified to allow
* for some argv items to be set after return.
*
* @param maxargc	maximum size of argv
* @param iargc		initial size of argv (number of leading, unassigned)
* @param fargc		pointer to final size of argv array, NULL excluded
* @param ap		va_list
* @return		NULL-terminated argv array
*/

char **
__russ_variadic_to_argv(int maxargc, int iargc, int *fargc, va_list ap) {
	va_list	ap2;
	char	**argv;
	int	i;

	va_copy(ap2, ap);
	for (i = iargc; (va_arg(ap2, char *) != NULL) && (i < maxargc); i++);
	va_end(ap2);

	if (i == maxargc) {
		return NULL;
	}
	if ((argv = malloc(sizeof(char *)*(i+1))) == NULL) {
		return NULL;
	}

	*fargc = i;
	va_copy(ap2, ap);
	for (i = iargc; i < *fargc; i++) {
		argv[i] = va_arg(ap, char*);
	}
	va_end(ap2);
	argv[i] = NULL;
	return argv;
}
