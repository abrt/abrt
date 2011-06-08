/*
 * Utility routines.
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
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
