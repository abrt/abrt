/*
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "libreport.h"

/* Like strcpy but can copy overlapping strings. */
void overlapping_strcpy(char *dst, const char *src)
{
    /* Cheap optimization for dst == src case -
     * better to have it here than in many callers.
     */
    if (dst != src)
    {
        while ((*dst = *src) != '\0')
        {
            dst++;
            src++;
        }
    }
}
