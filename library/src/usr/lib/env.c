/*
* lib/env.c
*/

/*
# license--start
#
# Copyright 2016 John Marshall
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

#include <stdlib.h>
#include <string.h>

extern char	**environ;

/**
* Cross-platform function to clear environment.
*
* @return		0 on success, -1 on failure
*/
int
russ_clearenv(void) {
#if defined(__APPLE__) || defined(__FreeBSD__)
	char	name[256], *_name = NULL;
	char	*ch;
	int	pos, rv;

	if (environ == NULL) {
		return 0;
	}

	while (*environ) {
		if (ch = strchr(*environ, '=')) {
			pos = ch-*environ;
		} else {
			pos = strlen(*environ)+1;
		}
		if (pos < sizeof(name)) {
			strncpy(name, *environ, pos);
			name[pos] = '\0';
			rv = unsetenv(name);
		} else {
			_name = strndup(*environ, pos);
			rv = unsetenv(_name);
			free(_name);
		}
		if (rv) {
			return -1;
		}
	}
	return 0;
#else
	return clearenv();
#endif
}
