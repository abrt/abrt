/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "libreport.h"

static const char hexdigits_locase[] = "0123456789abcdef";

/* Emit a string of hex representation of bytes */
char *bin2hex(char *dst, const char *str, int count)
{
	while (count) {
		unsigned char c = *str++;
		/* put lowercase hex digits */
		*dst++ = hexdigits_locase[c >> 4];
		*dst++ = hexdigits_locase[c & 0xf];
		count--;
	}
	return dst;
}

/* Convert "xxxxxxxx" hex string to binary, no more than COUNT bytes */
char *hex2bin(char *dst, const char *str, int count)
{
	/* Parts commented out with // allow parsing
	 * of strings like "xx:x:x:xx:xx:xx:xxxxxx"
	 * (IPv6, ethernet addresses and the like).
	 */
	errno = EINVAL;
	while (*str && count) {
		uint8_t val;
		uint8_t c;

		c = *str++;
		if (isdigit(c))
			val = c - '0';
		else if ((c|0x20) >= 'a' && (c|0x20) <= 'f')
			val = (c|0x20) - ('a' - 10);
		else
			return NULL;
		val <<= 4;
		c = *str;
		if (isdigit(c))
			val |= c - '0';
		else if ((c|0x20) >= 'a' && (c|0x20) <= 'f')
			val |= (c|0x20) - ('a' - 10);
		//else if (c == ':' || c == '\0')
		//	val >>= 4;
		else
			return NULL;

		*dst++ = val;
		//if (c != '\0')
			str++;
		//if (*str == ':')
		//	str++;
		count--;
	}
	errno = (*str ? ERANGE : 0);
	return dst;
}
