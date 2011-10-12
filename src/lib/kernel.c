/*
    Copyright (C) 2011  ABRT team
    Copyright (C) 2011  RedHat Inc

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

char *koops_extract_version(const char *linepointer)
{
    if (strstr(linepointer, "Pid")
     || strstr(linepointer, "comm")
     || strstr(linepointer, "CPU")
     || strstr(linepointer, "REGS")
     || strstr(linepointer, "EFLAGS")
    ) {
        char* start;
        char* end;

        start = strstr(linepointer, "2.6.");
        if (!start)
            start = strstr(linepointer, "3.");
        if (start)
        {
            end = strchr(start, ')');
            if (!end)
                end = strchrnul(start, ' ');
            return xstrndup(start, end-start);
        }
    }

    return NULL;
}
