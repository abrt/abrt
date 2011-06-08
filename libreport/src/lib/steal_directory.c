/*
    Copyright (C) 2011  ABRT Team
    Copyright (C) 2011  RedHat inc.

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

struct dump_dir *steal_directory(const char *base_dir, const char *dump_dir_name)
{
    const char *base_name = strrchr(dump_dir_name, '/');
    if (base_name)
        base_name++;
    else
        base_name = dump_dir_name;

    struct dump_dir *dd_dst;
    unsigned count = 100;
    char *dst_dir_name = concat_path_file(base_dir, base_name);
    while (1)
    {
        dd_dst = dd_create(dst_dir_name, (uid_t)-1, 0640);
        free(dst_dir_name);
        if (dd_dst)
            break;
        if (--count == 0)
        {
            error_msg("Can't create new dump dir in '%s'", base_dir);
            return NULL;
        }
        struct timeval tv;
        gettimeofday(&tv, NULL);
        dst_dir_name = xasprintf("%s/%s.%u", base_dir, base_name, (int)tv.tv_usec);
    }

    VERB1 log("Creating copy in '%s'", dd_dst->dd_dirname);
    if (copy_file_recursive(dump_dir_name, dd_dst->dd_dirname) < 0)
    {
        /* error. copy_file_recursive already emitted error message */
        /* Don't leave half-copied dir lying around */
        dd_delete(dd_dst);
        return NULL;
    }

    return dd_dst;
}
