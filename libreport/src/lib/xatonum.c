/*
 * Utility routines.
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Copyright (C) 2010  ABRT team
 * Copyright (C) 2010  RedHat Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "libreport.h"

unsigned xatou(const char *numstr)
{
    unsigned long r;
    int old_errno;
    char *e;

    if (*numstr < '0' || *numstr > '9')
        goto inval;

    old_errno = errno;
    errno = 0;
    r = strtoul(numstr, &e, 10);
    if (errno || numstr == e || *e != '\0' || r > UINT_MAX)
        goto inval; /* error / no digits / illegal trailing chars */
    errno = old_errno; /* Ok.  So restore errno. */
    return r;

inval:
    error_msg_and_die("invalid number '%s'", numstr);
}

int xatoi_positive(const char *numstr)
{
    unsigned r = xatou(numstr);
    if (r > (unsigned)INT_MAX)
        error_msg_and_die("invalid number '%s'", numstr);
    return r;
}

int xatoi(const char *numstr)
{
    unsigned r;

    if (*numstr != '-')
        return xatoi_positive(numstr);

    r = xatou(numstr + 1);
    if (r > (unsigned)INT_MAX + 1)
        error_msg_and_die("invalid number '%s'", numstr);
    return - (int)r;
}
