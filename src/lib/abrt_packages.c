/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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
#include "abrtlib.h"

/* cuts the name from the NVR format: foo-1.2.3-1.el6
   returns a newly allocated string
*/
char* get_package_name_from_NVR_or_NULL(const char* packageNVR)
{
    char* package_name = NULL;
    if (packageNVR != NULL)
    {
        VERB1 log("packageNVR %s", packageNVR);
        package_name = xstrdup(packageNVR);
        char *pos = strrchr(package_name, '-');
        if (pos != NULL)
        {
            *pos = 0;
            pos = strrchr(package_name, '-');
            if (pos != NULL)
            {
                *pos = 0;
            }
        }
    }
    return package_name;
}
