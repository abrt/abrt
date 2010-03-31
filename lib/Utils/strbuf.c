/*  -*-mode:c;c-file-style:"bsd";c-basic-offset:4;indent-tabs-mode:nil-*-
    String buffer implementation

    Copyright (C) 2009  RedHat inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "strbuf.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include "xfuncs.h"

struct strbuf *strbuf_new()
{
    struct strbuf *buf = xmalloc(sizeof(struct strbuf));
    buf->alloc = 8;
    buf->len = 0;
    buf->buf = xmalloc(buf->alloc);
    buf->buf[buf->len] = '\0';
    return buf;
}

void strbuf_free(struct strbuf *strbuf)
{
    free(strbuf->buf);
    free(strbuf);
}

void strbuf_free_nobuf(struct strbuf *strbuf)
{
    free(strbuf);
}


void strbuf_clear(struct strbuf *strbuf)
{
    assert(strbuf->alloc > 0);
    strbuf->len = 0;
    strbuf->buf[0] = '\0';
}

/* Ensures that the buffer can be extended by num characters
 * without touching malloc/realloc.
 */
static void strbuf_grow(struct strbuf *strbuf, int num)
{
    if (strbuf->len + num + 1 > strbuf->alloc)
    {
	while (strbuf->len + num + 1 > strbuf->alloc)
	    strbuf->alloc *= 2; /* huge grow = infinite loop */

	strbuf->buf = realloc(strbuf->buf, strbuf->alloc);
	if (!strbuf->buf)
	{
	    puts("Error while allocating memory for string buffer.");
	    exit(5);
	}
    }
}

struct strbuf *strbuf_append_char(struct strbuf *strbuf, char c)
{
    strbuf_grow(strbuf, 1);
    strbuf->buf[strbuf->len++] = c;
    strbuf->buf[strbuf->len] = '\0';
    return strbuf;
}

struct strbuf *strbuf_append_str(struct strbuf *strbuf, const char *str)
{
    int len = strlen(str);
    strbuf_grow(strbuf, len);
    assert(strbuf->len + len < strbuf->alloc);
    strcpy(strbuf->buf + strbuf->len, str);
    strbuf->len += len;
    return strbuf;
}

struct strbuf *strbuf_prepend_str(struct strbuf *strbuf, const char *str)
{
    int len = strlen(str);
    strbuf_grow(strbuf, len);
    assert(strbuf->len + len < strbuf->alloc);
    memmove(strbuf->buf + len, strbuf->buf, strbuf->len + 1);
    memcpy(strbuf->buf, str, len);
    return strbuf;
}

struct strbuf *strbuf_append_strf(struct strbuf *strbuf, const char *format, ...)
{
    va_list p;
    char *string_ptr;

    va_start(p, format);
    string_ptr = xvasprintf(format, p);
    va_end(p);

    strbuf_append_str(strbuf, string_ptr);
    free(string_ptr);
    return strbuf;
}

struct strbuf *strbuf_prepend_strf(struct strbuf *strbuf, const char *format, ...)
{
    va_list p;
    char *string_ptr;

    va_start(p, format);
    string_ptr = xvasprintf(format, p);
    va_end(p);

    strbuf_prepend_str(strbuf, string_ptr);
    free(string_ptr);
    return strbuf;
}
