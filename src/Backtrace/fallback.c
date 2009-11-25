/*
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
#include "fallback.h"
#include <stdlib.h>
#include <stdbool.h>

struct header
{
  struct strbuf *text;
  struct header *next;
};

static struct header *header_new()
{
  struct header *head = malloc(sizeof(struct header));
  if (!head)
  {
    puts("Error while allocating memory for backtrace header.");
    exit(5);
  }
  head->text = NULL;
  head->next = NULL;
  return head;
}

/* Recursively frees siblings. */
static void header_free(struct header *head)
{
  if (head->text)
    strbuf_free(head->text);
  if (head->next)
    header_free(head->next);
  free(head);
}

/* Inserts new header to array if it is not already there. */
static void header_set_insert(struct header *cur, struct strbuf *new)
{
  /* Duplicate found case. */
  if (strcmp(cur->text->buf, new->buf) == 0)
    return;

  /* Last item case, insert new header here. */
  if (cur->next == NULL)
  {
    cur->next = header_new();
    cur->next->text = new;
    return;
  }

  /* Move to next item in array case. */
  header_set_insert(cur->next, new);
}

struct strbuf *independent_backtrace(char *input)
{
  struct strbuf *header = strbuf_new();
  bool in_bracket = false;
  bool in_quote = false;
  bool in_header = false;
  bool in_digit = false;
  bool has_at = false;
  bool has_filename = false;
  bool has_bracket = false;
  struct header *headers = NULL;

  const char *bk = input;
  while (*bk)
  {
    if (bk[0] == '#'
	&& bk[1] >= '0' && bk[1] <= '7'
	&& bk[2] == ' ' /* take only #0...#7 (8 last stack frames) */
	&& !in_quote) 
    {
      if (in_header && !has_filename)
	strbuf_clear(header);
      in_header = true;
    }

    if (!in_header)
    {
      ++bk;
      continue;
    }

    if (isdigit(*bk) && !in_quote && !has_at)
      in_digit = true;
    else if (bk[0] == '\\' && bk[1] == '\"')
      bk++;
    else if (*bk == '\"')
      in_quote = in_quote == true ? false : true;
    else if (*bk == '(' && !in_quote)
    {
      in_bracket = true;
      in_digit = false;
      strbuf_append_char(header, '(');
    }
    else if (*bk == ')' && !in_quote)
    {
      in_bracket = false;
      has_bracket = true;
      in_digit = false;
      strbuf_append_char(header, '(');
    }
    else if (*bk == '\n' && has_filename)
    {
      if (headers == NULL)
      {
	headers = header_new();
	headers->text = header;
      }
      else
	header_set_insert(headers, header);

      header = strbuf_new();
      in_bracket = false;
      in_quote = false;
      in_header = false;
      in_digit = false;
      has_at = false;
      has_filename = false;
      has_bracket = false;
    }
    else if (*bk == ',' && !in_quote)
      in_digit = false;
    else if (isspace(*bk) && !in_quote)
      in_digit = false;
    else if (bk[0] == 'a' && bk[1] == 't' && has_bracket && !in_quote)
    {
      has_at = true;
      strbuf_append_char(header, 'a');
    }
    else if (bk[0] == ':' && has_at && isdigit(bk[1]) && !in_quote)
      has_filename = true;
    else if (in_header && !in_digit && !in_quote && !in_bracket)
      strbuf_append_char(header, *bk);
    
    bk++;
  }
  
  strbuf_free(header);

  struct strbuf *result = strbuf_new();
  struct header *loop = headers;
  while (loop)
  {
    strbuf_append_str(result, loop->text->buf);
    strbuf_append_char(result, '\n');
    loop = loop->next;
  }

  if (headers)
    header_free(headers); /* recursive */

  return result;
}
