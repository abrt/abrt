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

#include "libreport.h"

char *iso_date_string(time_t *pt)
{
    static char buf[sizeof("YYYY-MM-DD-HH:MM:SS") + 4];

    time_t t;
    struct tm *ptm = localtime(pt ? pt : (time(&t), &t));
    strftime(buf, sizeof(buf), "%Y-%m-%d-%H:%M:%S", ptm);

    return buf;
}
