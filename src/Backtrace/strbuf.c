/*
    strbuf.c - string buffer implementation

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
#include <assert.h>
#include <string.h>

struct strbuf *strbuf_new()
{
  struct strbuf *buf = malloc(sizeof(struct strbuf));
  if (!buf)
  {
    puts("Error while allocating memory for string buffer.");
    exit(5);
  }

  buf->alloc = 8;
  buf->len = 0;
  buf->buf = malloc(buf->alloc);
  if (!buf->buf)
  {
    puts("Error while allocating memory for string buffer.");
    exit(5);
  }
  
  buf->buf[buf->len] = '\0';
  return buf;
}

void strbuf_free(struct strbuf *buf)
{
  free(buf->buf);
  free(buf);
}

void strbuf_clear(struct strbuf *buf)
{
  assert(buf->alloc > 0);
  buf->len = 0;
  buf->buf[0] = '\0';
}

/* Ensures that the buffer can be extended by num characters
 * without touching malloc/realloc.
 */
static void strbuf_grow(struct strbuf *buf, int num)
{
  if (buf->len + num + 1 > buf->alloc)
  {
    while (buf->len + num + 1 > buf->alloc)
      buf->alloc *= 2; /* huge grow = infinite loop */

    buf->buf = realloc(buf->buf, buf->alloc);
    if (!buf->buf)
    {
      puts("Error while allocating memory for string buffer.");
      exit(5);
    }
  }
}

struct strbuf *strbuf_append_char(struct strbuf *buf, char c)
{
  strbuf_grow(buf, 1);
  buf->buf[buf->len++] = c;
  buf->buf[buf->len] = '\0';
  return buf;
}

struct strbuf *strbuf_append_str(struct strbuf *buf, char *str)
{
  int len = strlen(str);
  strbuf_grow(buf, len);
  assert(buf->len + len < buf->alloc);
  strcpy(buf->buf + buf->len, str);
  buf->len += len;
  return buf;
}
