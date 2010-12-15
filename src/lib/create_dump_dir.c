/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat inc.

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

struct dump_dir *create_dump_dir(crash_data_t *crash_data)
{
    char *path = xasprintf(LOCALSTATEDIR"/run/abrt/tmp-%lu-%lu", (long)getpid(), (long)time(NULL));
    struct dump_dir *dd = dd_create(path, getuid());
    free(path);
    if (!dd)
        return NULL;

    GHashTableIter iter;
    char *name;
    struct crash_item *value;
    g_hash_table_iter_init(&iter, crash_data);
    while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
    {
        if (name[0] == '.' || strchr(name, '/'))
        {
            error_msg("Crash data field name contains disallowed chars: '%s'", name);
            goto next;
        }

//FIXME: what to do with CD_FLAG_BINs??
        if (value->flags & CD_FLAG_BIN)
            goto next;

        dd_save_text(dd, name, value->content);
 next: ;
    }

    return dd;
}
