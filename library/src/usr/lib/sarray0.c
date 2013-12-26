/*
** lib/sarray0.c
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

#include <stdlib.h>
#include <string.h>

/**
* Count elements of NULL-terminated string array (not including
* NULL).
*
* @param arr		string array
* @param max_cnt	maximum # of items to look for
* @return		# of strings upto NULL; -1 if arr == NULL; max_cnt if reached
*/
int
russ_sarray0_count(char **arr, int max_cnt) {
	int	i;

	if (arr == NULL) {
		return -1;
	}
	for (i = 0; (i < max_cnt) && (arr[i] != NULL); i++);
	return i;
}

/**
* Duplicate a NULL-terminated string array.
*
* @param arr		source string array
* @param max_cnt	max # of elements supported
* @return		duplicated array
*/
char **
russ_sarray0_dup(char **arr, int max_cnt) {
	char	**dst;
	int	i, cnt;

	if (((cnt = russ_sarray0_count(arr, max_cnt)) < 0)
		|| (cnt == max_cnt)) {
		return NULL;
	}
	cnt++;

	if ((dst = malloc(sizeof(char *)*(cnt))) == NULL) {
		return NULL;
	}
	for (i = 0; i < cnt; i++) {
		if (arri] == NULL) {
			dst[i] = NULL;
		} else if ((dst[i] = strdup(arr[i])) == NULL) {
			goto free_dst;
		}
	}
	return dst;
free_dst:
	for (; i >= 0; i--) {
		free(dst[i]);
	}
	return NULL;
}

/**
* Free NULL-terminated string array.
*
* @param arr		NULL-terminated string array
*/
void
russ_sarray0_free(char **arr) {
	char	**p;

	if (arr) {
		for (p = arr; *p != NULL; p++) {
			free(*p);
		}
		free(arr);
	}
}
