/*
* encdec2.c
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "russ_priv.h"

char *
russ_dec2_H(char *b, uint16_t *v) {
	*v = (uint8_t)(b[0]) | ((uint8_t)(b[1])<<8);
	return b+2;
}

char *
russ_dec2_I(char *b, uint32_t *v) {
	*v = (uint8_t)(b[0]) | ((uint8_t)(b[1])<<8)
		| ((uint8_t)(b[2])<<16) | ((uint8_t)(b[3])<<24);
	return b+4;
}

char *
russ_dec2_i(char *b, int32_t *v) {
	uint32_t	v2;
	char		*b2;

	b2 = russ_dec2_I(b, &v2);
	*v = (int32_t)v2;
	return b2;
}

char *
russ_dec2_b(char *b, char **bp) {
	int	count;

	if (((b = russ_dec2_I(b, &count)) == NULL)
		|| ((*bp = malloc(count)) == NULL)) {
		return NULL;
	}
	memcpy(*bp, b, count);
	return b+count;
}

char *
russ_dec2_s(char *b, char **bp) {
	return russ_dec2_b(b, bp);
}

/**
* Shared function for decoding string arrays.
*/
static char *
_dec2_s_array0(char *b, char ***v, int *alen, int append_null) {
	char	**array, *s;
	int	_bcount, i;

	b = russ_dec2_I(b, alen);
	if (*alen > 0) {
		if (append_null) {
			array = malloc(sizeof(char *)*(*alen+1));
		} else {
			array = malloc(sizeof(char *)*(*alen));
		}
		if (array == NULL) {
			return NULL;
		}
		for (i = 0; i < *alen; i++) {
			b = russ_dec2_s(b, &s);
			if (b == NULL) {
				goto free_array;
			}
			array[i] = s;
		}
		if (append_null) {
			array[*alen] = NULL;
		}
	} else {
		array = NULL;
	}
	*v = array;
	return b;
free_array:
	for (; i >= 0; i--) {
		free(array[i]);
	}
	free(*array);
	return NULL;
}

/**
* Decode string array with implicit NULL sentinel.
*
* @param b	buffer
* @param[out] vpp	string array with NULL sentinel
* @return	new buffer position; NULL if failure
*/
char *
russ_dec2_s_array0(char *b, char ***vpp, int *alen) {
	return _dec2_s_array0(b, vpp, alen, 1);
}

/**
* Decode string array of fixed # of items.
*
* @param b	buffer
* @param[out] vpp	string array with NULL sentinel
* @return	new buffer position; NULL if failure
*/
char *
russ_dec2_s_arrayn(char *b, char ***vpp, int *alen) {
	return _dec2_s_array0(b, vpp, alen, 0);
}

/***** encoders *****/

/**
* Encode uint16.
*
* @param b	buffer
* @param bend	end of buffer
* @param v	uint16 value
* @return	new buffer position; NULL if failure
*/
char *
russ_enc2_H(char *b, char *bend, uint16_t v) {
	if ((bend-b) < 2) {
		return NULL;
	}
	b[0] = (uint8_t)v & 0xff;
	b[1] = (uint8_t)(v >> 8) & 0xff;
	return b+2;
}

/**
* Encode uint32.
*
* @param b	buffer
* @param bend	end of buffer
* @param v	uint32 value
* @return	new buffer position; NULL if failure
*/
char *
russ_enc2_I(char *b, char *bend, uint32_t v) {
	if ((bend-b) < 4) {
		return NULL;
	}
	b[0] = (uint8_t)v & 0xff;
	b[1] = (uint8_t)(v>>8) & 0xff;
	b[2] = (uint8_t)(v>>16) & 0xff;
	b[3] = (uint8_t)(v>>24) & 0xff;
	return b+4;
}

/**
* Encode int32.
*
* @param b	buffer
* @param bend	end of buffer
* @param v	int32 value
* @return	new buffer position; NULL if failure
*/
char *
russ_enc2_i(char *b, char *bend, int32_t v) {
	if ((bend-b) < 4) {
		return NULL;
	}
	b[0] = (uint8_t)v & 0xff;
	b[1] = (uint8_t)(v >> 8) & 0xff;
	b[2] = (uint8_t)(v >> 16) & 0xff;
	b[3] = (uint8_t)(v >> 24) & 0xff;
	return b+4;
}

/**
* Encode byte array.
* Encoding: alen(4) bytes(n)
*
* @param b	buffer
* @param bend	end of buffer
* @param v	array of bytes
* @return	new buffer position; NULL if failure
*/
char *
russ_enc2_bytes(char *b, char *bend, char *v, int alen) {
	if ((bend-b) < 4+alen) {
		return NULL;
	}
	b = russ_enc2_I(b, bend, alen);
	memcpy(b, v, alen);
	return b+alen;
}

/**
* Encode string (including \0).
* Encoding: alen(4) string(n)
*
* @param b	buffer
* @param bend	end of buffer
* @param v	string ending in \0
* @return	new buffer position; NULL if failure
*/
char *
russ_enc2_string(char *b, char *bend, char *v) {
	return russ_enc2_bytes(b, bend, v, strlen(v)+1);
}

/**
* Encode string array of fixed length.
* Encoding: alen(4b) [string [...]]
*
* @param b	buffer
* @param bend	end of buffer
* @param v	array of strings (ignored if alen==0)
* @param alen	# of array items
* @return	updated buffer position; NULL on failure
*/
char *
russ_enc2_s_arrayn(char *b, char *bend, char **v, int alen) {
	int	i;

	if ((b = russ_enc2_I(b, bend, alen)) == NULL) {
		return NULL;
	}
	for (i = 0; i < alen; i++) {
		if ((b = russ_enc2_string(b, bend, v[i])) == NULL) {
			return NULL;
		}
	}
	return b;
}

/**
* Encode string array having NULL sentinel. Calls russ_enc_s_arrayn.
* Sentinel is not encoded.
*
* @param b	buffer
* @param bend	end of buffer
* @param v	array of strings (may also be NULL)
* @return	updated buffer position; NULL on failure
*/
char *
russ_enc2_s_array0(char *b, char *bend, char **v) {
	int	alen;

	if (v == NULL) {
		alen = 0;
	} else {
		for (alen = 0; (alen < 16384) && (v[alen] != NULL); alen++);
		if (alen == 16384) {
			return NULL;
		}
	}
	return russ_enc2_s_arrayn(b, bend, v, alen);
}
