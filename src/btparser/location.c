/*
    location.c

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
#include "location.h"
#include <stdlib.h> /* contains NULL */

void
btp_location_init(struct btp_location *location)
{
    location->line = 1;
    location->column = 0;
    location->message = NULL;
}

void
btp_location_add(struct btp_location *location,
                 int add_line,
                 int add_column)
{
    btp_location_add_ext(&location->line,
                         &location->column,
                         add_line,
                         add_column);
}

void
btp_location_add_ext(int *line,
                     int *column,
                     int add_line,
                     int add_column)
{
    if (add_line > 1)
    {
        *line += add_line - 1;
        *column = add_column;
    }
    else
        *column += add_column;
}

void
btp_location_eat_char(struct btp_location *location,
                      char c)
{
    btp_location_eat_char_ext(&location->line,
                              &location->column,
                              c);
}

void
btp_location_eat_char_ext(int *line,
                          int *column,
                          char c)
{
    if (c == '\n')
    {
        *line += 1;
        *column = 0;
    }
    else
        *column += 1;
}
