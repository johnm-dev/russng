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
