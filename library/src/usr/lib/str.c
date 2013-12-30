/*
** lib/str.c
*/

/*
# license--start
#
# Copyright 2012-2013 John Marshall
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

/**
* Count the number of substrings in the string.
*
* @param s		string
* @param ss		substring
* @return		# of instances found
*/
int
russ_str_count_sub(char *s, char *ss) {
	int	ss_len, n;

	ss_len = strlen(ss);
	for (n = 0; s != NULL; n++) {
		if ((s = strstr(s, ss)) == NULL) {
			break;
		}
		s += ss_len;
	}
	return n;
}

/**
* Find the idx-th component from the string as divided by a
* separator and return a copy of it. Must be freed by caller.
*
* If string starts with separator, then component at index 0 returns
* "" (empty string).
*
* @param s		string
* @param sep		separator character
* @param idx		index of the desired component
* @return		component string; NULL on error/unavailable
*/
char *
russ_str_get_comp(char *s, char sep, int idx) {
	char	*p;

	for (; idx > 0; idx--) {
		if ((s = strchr(s, sep)) == NULL) {
			return NULL;
		}
		s++;
	}
	if ((p = strchr(s, sep)) == NULL) {
		s = strdup(s);
	} else {
		s = strndup(s, p-s);
	}
	return s;
}
