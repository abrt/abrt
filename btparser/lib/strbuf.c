/*
    strbuf.c - string buffer

    Copyright (C) 2010  Red Hat, Inc.

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
#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

struct btp_strbuf *
btp_strbuf_new()
{
    struct btp_strbuf *buf = btp_malloc(sizeof(struct btp_strbuf));
    buf->alloc = 8;
    buf->len = 0;
    buf->buf = btp_malloc(buf->alloc);
    buf->buf[buf->len] = '\0';
    return buf;
}

void
btp_strbuf_free(struct btp_strbuf *strbuf)
{
    if (!strbuf)
        return;

    free(strbuf->buf);
    free(strbuf);
}

char *
btp_strbuf_free_nobuf(struct btp_strbuf *strbuf)
{
    char *buf = strbuf->buf;
    free(strbuf);
    return buf;
}


void
btp_strbuf_clear(struct btp_strbuf *strbuf)
{
    assert(strbuf->alloc > 0);
    strbuf->len = 0;
    strbuf->buf[0] = '\0';
}

/* Ensures that the buffer can be extended by num characters
 * without touching malloc/realloc.
 */
void
btp_strbuf_grow(struct btp_strbuf *strbuf, int num)
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

struct btp_strbuf *
btp_strbuf_append_char(struct btp_strbuf *strbuf,
                       char c)
{
    btp_strbuf_grow(strbuf, 1);
    strbuf->buf[strbuf->len++] = c;
    strbuf->buf[strbuf->len] = '\0';
    return strbuf;
}

struct btp_strbuf *
btp_strbuf_append_str(struct btp_strbuf *strbuf,
                      const char *str)
{
    int len = strlen(str);
    btp_strbuf_grow(strbuf, len);
    assert(strbuf->len + len < strbuf->alloc);
    strcpy(strbuf->buf + strbuf->len, str);
    strbuf->len += len;
    return strbuf;
}

struct btp_strbuf *
btp_strbuf_prepend_str(struct btp_strbuf *strbuf,
                       const char *str)
{
    int len = strlen(str);
    btp_strbuf_grow(strbuf, len);
    assert(strbuf->len + len < strbuf->alloc);
    memmove(strbuf->buf + len, strbuf->buf, strbuf->len + 1);
    memcpy(strbuf->buf, str, len);
    strbuf->len += len;
    return strbuf;
}

struct btp_strbuf *
btp_strbuf_append_strf(struct btp_strbuf *strbuf,
                       const char *format, ...)
{
    va_list p;
    char *string_ptr;

    va_start(p, format);
    string_ptr = btp_vasprintf(format, p);
    va_end(p);

    btp_strbuf_append_str(strbuf, string_ptr);
    free(string_ptr);
    return strbuf;
}

struct btp_strbuf *
btp_strbuf_prepend_strf(struct btp_strbuf *strbuf,
                        const char *format, ...)
{
    va_list p;
    char *string_ptr;

    va_start(p, format);
    string_ptr = btp_vasprintf(format, p);
    va_end(p);

    btp_strbuf_prepend_str(strbuf, string_ptr);
    free(string_ptr);
    return strbuf;
}
