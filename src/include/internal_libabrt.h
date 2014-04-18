/*
    Copyright (C) 2014  ABRT team
    Copyright (C) 2014  RedHat Inc

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

#include "libabrt.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#undef NORETURN
#define NORETURN __attribute__ ((noreturn))

/* Must be after #include "config.h" */
#if ENABLE_NLS
# include <libintl.h>
# define _(S) dgettext(PACKAGE, S)
#else
# define _(S) (S)
#endif

extern int g_libabrt_inited;
void libabrt_init(void);

#define INITIALIZE_LIBABRT() \
    do \
    { \
        if (!g_libabrt_inited) \
        { \
            g_libabrt_inited = 1; \
            libabrt_init(); \
        } \
    } \
    while (0)

