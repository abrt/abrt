/*
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
#include "libreport.h"

int prefixcmp(const char *str, const char *prefix)
{
    for (; ; str++, prefix++)
        if (!*prefix)
            return 0;
        else if (*str != *prefix)
            return (unsigned char)*prefix - (unsigned char)*str;
}

int suffixcmp(const char *str, const char *suffix)
{
    int len_minus_suflen = strlen(str) - strlen(suffix);
    if (len_minus_suflen < 0)
        return len_minus_suflen;
    else
        return strcmp(str + len_minus_suflen, suffix);
}

/*
 * Trims whitespace characters both from left and right side of a string.
 * Modifies the string in-place. Returns the trimmed string.
 */
char *strtrim(char *str)
{
    if (!str)
        return NULL;

    // Remove leading spaces.
    overlapping_strcpy(str, skip_whitespace(str));

    // Remove trailing spaces.
    int i = strlen(str);
    while (--i >= 0)
    {
        if (!isspace(str[i]))
            break;
    }
    str[++i] = '\0';
    return str;
}

struct strbuf *strbuf_new(void)
{
    struct strbuf *buf = xzalloc(sizeof(*buf));
    /*buf->len = 0; - done by xzalloc */
    buf->alloc = 8;
    buf->buf = xzalloc(8);
    return buf;
}

void strbuf_free(struct strbuf *strbuf)
{
    if (!strbuf)
        return;
    free(strbuf->buf);
    free(strbuf);
}

char *strbuf_free_nobuf(struct strbuf *strbuf)
{
    char *ret = strbuf->buf;
    free(strbuf);
    return ret;
}


void strbuf_clear(struct strbuf *strbuf)
{
    assert(strbuf->alloc > 0);
    strbuf->len = 0;
    strbuf->buf[0] = '\0';
}

/* Ensures that the buffer can be extended by N+1 characters
 * without touching malloc/realloc.
 * Returns pointer where appended chars can be stored by the caller;
 * increments ->len by N (therefore callers don't need to do it).
 */
static char *strbuf_grow(struct strbuf *strbuf, unsigned increment)
{
    unsigned len = strbuf->len;
    unsigned need = strbuf->len = len + increment;
    unsigned cur_size = strbuf->alloc;
    if (cur_size <= need)
    {
        while (cur_size <= need)
            cur_size += 64 + cur_size / 8;
        strbuf->alloc = cur_size;
        strbuf->buf = xrealloc(strbuf->buf, cur_size);
    }
    char *p = strbuf->buf + len;
    return p;
}

struct strbuf *strbuf_append_char(struct strbuf *strbuf, char c)
{
    char *p = strbuf_grow(strbuf, 1);
    *p++ = c;
    *p = '\0';
    return strbuf;
}

struct strbuf *strbuf_append_str(struct strbuf *strbuf, const char *str)
{
    unsigned len = strlen(str);
    char *p = strbuf_grow(strbuf, len);
    assert(strbuf->len < strbuf->alloc);
    strcpy(p, str);
    return strbuf;
}

struct strbuf *strbuf_prepend_str(struct strbuf *strbuf, const char *str)
{
    unsigned cur_len = strbuf->len;
    unsigned inc_len = strlen(str);
    strbuf_grow(strbuf, inc_len);
    assert(strbuf->len < strbuf->alloc);
    memmove(strbuf->buf + inc_len, strbuf->buf, cur_len);
    memcpy(strbuf->buf, str, inc_len);
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
