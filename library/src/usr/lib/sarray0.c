/*
* lib/sarray0.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <russ_priv.h>

/**
* Create empty NULL-terminated string array.
*
* @param n		new array size (not including the terminating NULL)
* @return		new string array; NULL on failure
*/
static char **
_russ_sarray0_new(int n) {
	char	**self;
	int	i;

	if ((self = russ_malloc(sizeof(char *)*(n+1))) == NULL) {
		return NULL;
	}
	for (i = 0; i <= n; i++) {
		self[i] = NULL;
	}
	return self;
}

/**
* Create NULL-terminated string array.
*
* @param n		new array size (not including the terminating NULL)
* @param ...		strings (at least n of them)
* @return		new string array; NULL on failure
*/
char **
russ_sarray0_new(int n, ...) {
	va_list	ap;
	char	**self, *s;
	int	i;

	if ((self = _russ_sarray0_new(n)) == NULL) {
		return NULL;
	}

	va_start(ap, n);
	for (i = 0; i < n; i++) {
		if ((s = va_arg(ap, char *)) == NULL) {
			break;
		}
		if ((self[i] = strdup(s)) == NULL) {
			goto freeall;
		}
	}
	va_end(ap);

	return self;
freeall:
	va_end(ap);
	self = russ_sarray0_free(self);
	return NULL;
}

/**
* Create NULL-terminated string array from split string.
*
* @param s		string to split
* @param ss		string used for split
* @param sindex		starting index from which to copy elements
* @return		new string array; NULL on failure
*/
char **
russ_sarray0_new_split(char *s, char *ss, int sindex) {
	char	**self;
	char	*p, *pp;
	int	i, n, ss_len;

	ss_len = strlen(ss);

	n = russ_str_count_sub(s, ss)+1;
	n = ((sindex < 0) || (n < sindex)) ? 0 : n-sindex;
	if ((self = _russ_sarray0_new(n)) == NULL) {
		return NULL;
	}
	if (n > 0) {
		for (i = -sindex, p = s; i < n; i++) {
			pp = strstr(p, ss);
			if (pp == NULL) {
				s = strdup(p);
			} else {
				s = strndup(p, pp-p);
				p = pp+ss_len;
			}
			if (s == NULL) {
				goto freeall;
			}
			if (i >= 0) {
				self[i] = s;
			}
		}
	}
	return self;
freeall:
	return russ_sarray0_free(self);
}

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
			*p = russ_free(*p);
		}
		arr = russ_free(arr);
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

	if ((dst = russ_malloc(sizeof(char *)*(cnt))) == NULL) {
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
		dst[i] = russ_free(dst[i]);
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
* Get suffix from (first) element with matching prefix.
*
* The returned pointer points into the element.
*
* @param arr		string array
* @param prefix		string
* @return		pointer to value in element; NULL if not found
*/
char *
russ_sarray0_get_suffix(char **arr, char *prefix) {
	int	i;

	if ((i = russ_sarray0_find_prefix(arr, prefix)) < 0) {
		return NULL;
	}
	return &arr[i][strlen(prefix)];
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
	arr[i] = russ_free(arr[i]);
	arr[i] = s;
	return 0;
free_s:
	s = russ_free(s);
	return -1;
}
