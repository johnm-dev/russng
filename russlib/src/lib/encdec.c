/*
* russ_encdec.c
*
* encode and decode into/from byte strings.
*
* Encoding is in big-endian. Strings and byte strings are encoded
* with size(4b) then the string/bytes.
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint16_t
russ_dec_H(char *b, int *count) {
	/* printf("(%hhu) (%hhu)\n", b[0], b[1]); */
	*count = 2;
	return (uint8_t)(b[0])
		| ((uint8_t)(b[1])<<8);
}

uint32_t
russ_dec_I(char *b, int *count) {
	/* printf("(%hhu) (%hhu) (%hhu) (%hhu)\n", b[0], b[1], b[2], b[3]); */
	*count = 4;
	return (uint8_t)(b[0])
		| ((uint8_t)(b[1])<<8)
		| ((uint8_t)(b[2])<<16)
		| ((uint8_t)(b[3])<<24);
}

int32_t
russ_dec_i(char *b, int *count) {
	/* printf("(%hhu) (%hhu) (%hhu) (%hhu)\n", b[0], b[1], b[2], b[3]); */
	*count = 4;
	return (uint8_t)b[0]
		| ((uint8_t)(b[1])<<8)
		| ((uint8_t)(b[2])<<16)
		| ((uint8_t)(b[3])<<24);
}

char *
russ_dec_b(char *b, int *count) {
	char	*dst;
	int	_count;

	_count = russ_dec_I(b, count);
	if (dst = malloc(_count)) {
		memcpy(dst, b+*count, _count);
		*count = *count+_count;
	}
	return dst;
}

char *
russ_dec_s(char *b, int *count) {
	return russ_dec_b(b, count);
}

char *
russ_enc_H(char *b, uint16_t v, int buf_size) {
	if (buf_size < 2) {
		return NULL;
	}

	b[0] = (uint8_t)v & 0xff;
	b[1] = (uint8_t)(v >> 8) & 0xff;
	/* printf("(%hu) (%hhu) (%hhu)\n", v, b[0], b[1]); */
	return b+2;
}

char *
russ_enc_I(char *b, uint32_t v, int buf_size) {
	if (buf_size < 4) {
		return NULL;
	}

	b[0] = (uint8_t)v & 0xff;
	b[1] = (uint8_t)(v >> 8) & 0xff;
	b[2] = (uint8_t)(v >> 16) & 0xff;
	b[3] = (uint8_t)(v >> 24) & 0xff;
	/* printf("(%hu) (%hhu) (%hhu) (%hhu) (%hhu)\n", v, b[0], b[1], b[2], b[3]); */
	return b+4;
}

char *
russ_enc_i(char *b, int32_t v, int buf_size) {
	if (buf_size < 4) {
		return NULL;
	}

	b[0] = (uint8_t)v & 0xff;
	b[1] = (uint8_t)(v >> 8) & 0xff;
	b[2] = (uint8_t)(v >> 16) & 0xff;
	b[3] = (uint8_t)(v >> 24) & 0xff;
	/* printf("(%hu) (%hhu) (%hhu) (%hhu) (%hhu)\n", v, b[0], b[1], b[2], b[3]); */
	return b+4;
}

/* encoding: size(4b) bytes(n) */
char *
russ_enc_bytes(char *b, char *s, int count, int buf_size) {
	if (buf_size >= 4+count) {
		b = russ_enc_I(b, count, buf_size);
		memcpy(b, s, count);
		return b+count;
	} else {
		return b;
	}
}

char *
russ_enc_string(char *b, char *s, int buf_size) {
	return russ_enc_bytes(b, s, strlen(s), buf_size);
}

