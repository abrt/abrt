/*
    strbuf.h - string buffer

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
#ifndef STRBUF_H
#define STRBUF_H

#ifdef __cplusplus
extern "C" {
#endif

struct strbuf
{
    /* Size of the allocated buffer. Always > 0. */
    int alloc;
    /* Length of the message, without the ending \0. */
    int len;
    char *buf;
};

extern struct strbuf *strbuf_new();
extern void strbuf_free(struct strbuf *strbuf);
/* Releases strbuf, but not the internal buffer. */
extern char* strbuf_free_nobuf(struct strbuf *strbuf);
extern void strbuf_clear(struct strbuf *strbuf);
extern struct strbuf *strbuf_append_char(struct strbuf *strbuf, char c);
extern struct strbuf *strbuf_append_str(struct strbuf *strbuf, const char *str);
extern struct strbuf *strbuf_prepend_str(struct strbuf *strbuf, const char *str);
extern struct strbuf *strbuf_append_strf(struct strbuf *strbuf, const char *format, ...);
extern struct strbuf *strbuf_prepend_strf(struct strbuf *strbuf, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
