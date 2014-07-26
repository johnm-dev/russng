/*
* lib/encdec.c
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "russ_priv.h"

/**
* Decode LE 16-bit unsigned integer.
*
* @param b		buffer
* @param[out] v		storage area
* @return		new buffer position; NULL if failure
*/
char *
russ_dec_H(char *b, uint16_t *v) {
	*v = (uint16_t)(uint8_t)b[0]
		| (uint16_t)(uint8_t)b[1]<<8;
	return b+2;
}

/**
* Decode LE 16-bit signed integer.
*
* @param b		buffer
* @param[out] v		storage area
* @return		new buffer position; NULL if failure
*/
char *
russ_dec_h(char *b, int16_t *v) {
	*v = (int16_t)(uint8_t)b[0]
		| (int16_t)(uint8_t)b[1]<<8;
	return b+2;
}

/**
* Decode LE 32-bit unsigned integer.
*
* @param b		buffer
* @param[out] v		storage area
* @return		new buffer position; NULL if failure
*/
char *
russ_dec_I(char *b, uint32_t *v) {
	*v = (uint32_t)(uint8_t)b[0]
		| (uint32_t)(uint8_t)b[1]<<8
		| (uint32_t)(uint8_t)b[2]<<16
		| (uint32_t)(uint8_t)b[3]<<24;
	return b+4;
}

/**
* Decode LE 32-bit signed integer.
*
* @param b		buffer
* @param[out] v		storage area
* @return		new buffer position; NULL if failure
*/
char *
russ_dec_i(char *b, int32_t *v) {
	uint32_t	v2;
	char		*b2;

	b2 = russ_dec_I(b, &v2);
	*v = (int32_t)v2;
	return b2;
}

/**
* Decode LE 64-bit unsigned integer.
*
* @param b		buffer
* @param[out] v		storage area
* @return		new buffer position; NULL if failure
*/
char *
russ_dec_Q(char *b, uint64_t *v) {
	*v = (uint64_t)(uint8_t)b[0]
		| (uint64_t)(uint8_t)b[1]<<8
		| (uint64_t)(uint8_t)b[2]<<16
		| (uint64_t)(uint8_t)b[3]<<24
		| (uint64_t)(uint8_t)b[4]<<32
		| (uint64_t)(uint8_t)b[5]<<40
		| (uint64_t)(uint8_t)b[6]<<48
		| (uint64_t)(uint8_t)b[7]<<56;
	return b+8;
}

/**
* Decode LE 64-bit signed integer.
*
* @param b		buffer
* @param[out] v		storage area
* @return		new buffer position; NULL if failure
*/
char *
russ_dec_q(char *b, int64_t *v) {
	uint64_t	v2;
	char		*b2;

	b2 = russ_dec_Q(b, &v2);
	*v = (int64_t)v2;
	return b2;
}

/**
* Decode size-encoded byte-array.
*
* @param b		buffer
* @param[out] bp	storage area
* @return		new buffer position; NULL if failure
*/
char *
russ_dec_b(char *b, char **bp) {
	int	count;

	if (((b = russ_dec_i(b, &count)) == NULL)
		|| ((*bp = russ_malloc(count)) == NULL)) {
		return NULL;
	}
	if (count > 0) {
		memcpy(*bp, b, count);	
	}
	return b+count;
}

/**
* Decode size-encoded char-array.
*
* @param b		buffer
* @param[out] bp	storage area
* @return		new buffer position; NULL if failure
*/
char *
russ_dec_s(char *b, char **bp) {
	return russ_dec_b(b, bp);
}

/**
* Shared decoder of NULL-terminated string arrays.
*
* @param b		buffer
* @param[out] v		storage area
* @param[out] alen	array length
* @param append_null	flag to append NULL
* @return		new buffer position; NULL if failure
*/
static char *
_dec_sarray0(char *b, char ***v, int *alen, int append_null) {
	char	**array, *s;
	int	_bcount, i;

	b = russ_dec_i(b, alen);
	if (*alen > 0) {
		if (append_null) {
			array = russ_malloc(sizeof(char *)*(*alen+1));
		} else {
			array = russ_malloc(sizeof(char *)*(*alen));
		}
		if (array == NULL) {
			return NULL;
		}
		for (i = 0; i < *alen; i++) {
			b = russ_dec_s(b, &s);
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
		array[i] = russ_free(array[i]);
	}
	*array = russ_free(*array);
	return NULL;
}

/**
* Decode string array with implicit NULL sentinel.
*
* @param b		buffer
* @param[out] vpp	string array with NULL sentinel
* @param[out] alen	length of array (including sentinel)
* @return		new buffer position; NULL if failure
*/
char *
russ_dec_sarray0(char *b, char ***vpp, int *alen) {
	return _dec_sarray0(b, vpp, alen, 1);
}

/**
* Decode string array of fixed # of items.
*
* @param b		buffer
* @param[out] vpp	string array with NULL sentinel
* @param[out] alen	length of array (including sentinel)
* @return		new buffer position; NULL if failure
*/
char *
russ_dec_sarrayn(char *b, char ***vpp, int *alen) {
	return _dec_sarray0(b, vpp, alen, 0);
}

/**
* Decode exit status.
* @param b		buffer
* @param[out] exitst	exit status
* @return		new buffer position; NULL if failure
*/
char *
russ_dec_exit(char *b, int *v) {
	return russ_dec_i(b, v);
}

/**
* Decode russ request object.
*
* @param b		buffer
* @param[out] vpp	request object
* @return		new buffer position; NULL if failure
*/
char *
russ_dec_req(char *b, struct russ_req **v) {
	struct russ_req	*req = NULL;
	char		*dummy = NULL;
	int		sz;

	if ((v == NULL)
		|| ((req = russ_req_new(NULL, NULL, NULL, NULL, NULL)) == NULL)) {
		return NULL;
	}
	if (((b = russ_dec_i(b, &sz)) == NULL)
		|| ((b = russ_dec_s(b, &(req->protocolstring))) == NULL)
		|| (strcmp(RUSS_REQ_PROTOCOLSTRING, req->protocolstring) != 0)
		|| ((b = russ_dec_b(b, &dummy)) == NULL)
		|| ((b = russ_dec_s(b, &(req->spath))) == NULL)
		|| ((b = russ_dec_s(b, &(req->op))) == NULL)
		|| ((b = russ_dec_sarray0(b, &(req->attrv), &sz)) == NULL)
		|| ((b = russ_dec_sarray0(b, &(req->argv), &sz)) == NULL)) {
		dummy = russ_free(dummy);
		req = russ_req_free(req);
		return NULL;
	}
	dummy = russ_free(dummy);
	req->opnum = russ_optable_find_opnum(NULL, req->op);
	*v = req;
	return b;
}

/***** encoders *****/

/**
* Encode uint16.
*
* @param b		buffer
* @param bend		end of buffer
* @param v		uint16 value
* @return		new buffer position; NULL if failure
*/
char *
russ_enc_H(char *b, char *bend, uint16_t v) {
	if ((bend-b) < 2) {
		return NULL;
	}
	b[0] = (uint8_t)v;
	b[1] = (uint8_t)(v>>8);
	return b+2;
}

/**
* Encode int16.
*
* @param b		buffer
* @param bend		end of buffer
* @param v		int16 value
* @return		new buffer position; NULL if failure
*/
char *
russ_enc_h(char *b, char *bend, int16_t v) {
	if ((bend-b) < 2) {
		return NULL;
	}
	b[0] = (uint8_t)v;
	b[1] = (uint8_t)(v>>8);
	return b+2;
}

/**
* Encode uint32.
*
* @param b		buffer
* @param bend		end of buffer
* @param v		uint32 value
* @return		new buffer position; NULL if failure
*/
char *
russ_enc_I(char *b, char *bend, uint32_t v) {
	if ((bend-b) < 4) {
		return NULL;
	}
	b[0] = (uint8_t)v;
	b[1] = (uint8_t)(v>>8);
	b[2] = (uint8_t)(v>>16);
	b[3] = (uint8_t)(v>>24);
	return b+4;
}

/**
* Encode int32.
*
* @param b		buffer
* @param bend		end of buffer
* @param v		int32 value
* @return		new buffer position; NULL if failure
*/
char *
russ_enc_i(char *b, char *bend, int32_t v) {
	if ((bend-b) < 4) {
		return NULL;
	}
	b[0] = (uint8_t)v;
	b[1] = (uint8_t)(v>>8);
	b[2] = (uint8_t)(v>>16);
	b[3] = (uint8_t)(v>>24);
	return b+4;
}

/**
* Encode uint64.
*
* @param b		buffer
* @param bend		end of buffer
* @param v		uint64 value
* @return		new buffer position; NULL if failure
*/
char *
russ_enc_Q(char *b, char *bend, uint64_t v) {
	if ((bend-b) < 8) {
		return NULL;
	}
	b[0] = (uint8_t)v;
	b[1] = (uint8_t)(v>>8);
	b[2] = (uint8_t)(v>>16);
	b[3] = (uint8_t)(v>>24);
	b[4] = (uint8_t)(v>>32);
	b[5] = (uint8_t)(v>>40);
	b[6] = (uint8_t)(v>>48);
	b[7] = (uint8_t)(v>>56);
	return b+8;
}

/**
* Encode int64.
*
* @param b		buffer
* @param bend		end of buffer
* @param v		int64 value
* @return		new buffer position; NULL if failure
*/
char *
russ_enc_q(char *b, char *bend, int64_t v) {
	if ((bend-b) < 8) {
		return NULL;
	}
	b[0] = (uint8_t)v;
	b[1] = (uint8_t)(v>>8);
	b[2] = (uint8_t)(v>>16);
	b[3] = (uint8_t)(v>>24);
	b[4] = (uint8_t)(v>>32);
	b[5] = (uint8_t)(v>>40);
	b[6] = (uint8_t)(v>>48);
	b[7] = (uint8_t)(v>>56);
	return b+8;
}

/**
* Encode byte array.
*
* Encoding: alen(4) bytes(n)
*
* @param b		buffer
* @param bend		end of buffer
* @param v		array of bytes
* @param alen		length of array
* @return		new buffer position; NULL if failure
*/
char *
russ_enc_b(char *b, char *bend, char *v, int alen) {
	if ((bend-b) < 4+alen) {
		return NULL;
	}
	b = russ_enc_i(b, bend, alen);
	if (alen > 0) {
		memcpy(b, v, alen);
	}
	return b+alen;
}

/**
* Encode string (including \0).
*
* Encoding: alen(4) string(n)
*
* @param b		buffer
* @param bend		end of buffer
* @param v		string ending in \0
* @return		new buffer position; NULL if failure
*/
char *
russ_enc_s(char *b, char *bend, char *v) {
	return russ_enc_b(b, bend, v, strlen(v)+1);
}

/**
* Encode string array of fixed length.
*
* Encoding: alen(4b) [string [...]]
*
* @param b		buffer
* @param bend		end of buffer
* @param v		array of strings (ignored if alen==0)
* @param alen		# of array items
* @return		new buffer position; NULL on failure
*/
char *
russ_enc_sarrayn(char *b, char *bend, char **v, int alen) {
	int	i;

	if ((b = russ_enc_i(b, bend, alen)) == NULL) {
		return NULL;
	}
	for (i = 0; i < alen; i++) {
		if ((b = russ_enc_s(b, bend, v[i])) == NULL) {
			return NULL;
		}
	}
	return b;
}

/**
* Encode string array having NULL sentinel.
*
* Calls russ_enc_sarrayn(). Sentinel is not encoded.
*
* @param b		buffer
* @param bend		end of buffer
* @param v		array of strings (may also be NULL)
* @return		new buffer position; NULL on failure
*/
char *
russ_enc_sarray0(char *b, char *bend, char **v) {
	int	alen;

	if (v == NULL) {
		alen = 0;
	} else {
		for (alen = 0; (alen < 16384) && (v[alen] != NULL); alen++);
		if (alen == 16384) {
			return NULL;
		}
	}
	return russ_enc_sarrayn(b, bend, v, alen);
}

/**
* Encode exit status.
*
* @param b		buffer
* @param bend		end of buffer
* @param v		exit status
* @return		new buffer position; NULL on failure
*/
char *
russ_enc_exit(char *b, char *bend, int v) {
	return russ_enc_i(b, bend, v);
}

/**
* Encode russ request object.
*
* @param b		buffer
* @param bend		end of buffer
* @param v		request object
* @return		new buffer position; NULL on failure
*/
char *
russ_enc_req(char *b, char *bend, struct russ_req *v) {
	char	*_b = b;

	if ((v == NULL)
		|| ((b = russ_enc_i(b, bend, 0)) == NULL)
		|| ((b = russ_enc_s(b, bend, v->protocolstring)) == NULL)
		|| ((b = russ_enc_b(b, bend, NULL, 0)) == NULL) /* dummy */
		|| ((b = russ_enc_s(b, bend, v->spath)) == NULL)
		|| ((b = russ_enc_s(b, bend, v->op)) == NULL)
		|| ((b = russ_enc_sarray0(b, bend, v->attrv)) == NULL)
		|| ((b = russ_enc_sarray0(b, bend, v->argv)) == NULL)) {
		return NULL;
	}
	/* patch size */
	russ_enc_i(_b, bend, b-_b-4);
	return b;
}
