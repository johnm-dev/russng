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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
* Free NULL-terminated string array.
*
* @param arr		NULL-terminated string array
* @return		NULL
*/
char **
russ_sarray0_free(char **arr) {
	char	**p;

	if (arr) {
		for (p = arr; *p != NULL; p++) {
			free(*p);
		}
		free(arr);
	}
	return NULL;
}

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
		if (arr[i] == NULL) {
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
* Find element matching string.
*
* @param arr		string array
* @param s		string
* @return		index into array; -1 if not found
*/
int
russ_sarray0_find(char **arr, char *s) {
	int	i;

	if ((arr == NULL) || (s == NULL)) {
		return -1;
	}
	for (i = 0; arr[i] != NULL; i++) {
		if (strcmp(arr[i], s) == 0) {
			return i;
		}
	}
	return -1;
}

/**
* Find element starting with a prefix.
*
* @param arr		string array
* @param prefix		string
* @return		index into array; -1 if not found
*/
int
russ_sarray0_find_prefix(char **arr, char *prefix) {
	int	len;
	int	i;

	if ((arr == NULL) || (prefix == NULL)) {
		return -1;
	}
	len = strlen(prefix);
	for (i = 0; arr[i] != NULL; i++) {
		if (strncmp(arr[i], prefix, len) == 0) {
			return i;
		}
	}
	return -1;
}

/**
* Remove element at index; or, move elements above index to
* positions one less than they are at.
*
* The underlying array size does not change.
*
* @param arr		string array
* @param index		index of element to remove
* @return		0 on success; -1 on failure
*/
int
russ_sarray0_remove(char **arr, int index) {
	int	i;

	if (arr == NULL) {
		return -1;
	}
	for (i = 0; i <= index; i++) {
		if (arr[i] == NULL) {
			return -1;
		}
	}
	for (; arr[i] != NULL; i++) {
		arr[i] = arr[i+1];
	}
	return 0;
}

/**
* Update array element at index or add.
*
* If string array is NULL, then a new array may be allocated and
* returned.
*
* @param arrp[inout]	pointer to string array (array may be NULL)
* @param index		index of element to update
* @param s		new string
* @return		0 on success; -1 on failure
*/
int
russ_sarray0_update(char ***arrp, int index, char *s) {
	char	**arr;
	int	len;
	int	i;

	arr = *arrp;
	if ((s == NULL) || ((s = strdup(s)) == NULL)) {
		return -1;
	}
	if (index >= 0) {
		/* check if index within range (before NULL) */
		if (arr == NULL) {
			goto free_s;
		}
		for (i = 0; i <= index; i++) {
			if (arr[i] == NULL) {
				goto free_s;
			}
		}
		i = index;
	} else {
		/* resize; set NULL */
		i = 0;
		if (arr != NULL) {
			for (i = 0; arr[i] != NULL; i++);
		}
		if ((arr = realloc(arr, sizeof(char *)*(i+2))) == NULL) {
			goto free_s;
		}
		arr[i+1] = NULL;
		*arrp = arr;
	}
	/* set */
	free(arr[i]);
	arr[i] = s;
	return 0;
free_s:
	free(s);
	return -1;
}