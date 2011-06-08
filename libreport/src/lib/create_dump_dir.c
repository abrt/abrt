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

#include "libreport.h"

static struct dump_dir *try_dd_create(const char *base_dir_name, const char *dir_name)
{
    char *path = concat_path_file(base_dir_name, dir_name);
    struct dump_dir *dd = dd_create(path, (uid_t)-1L, 0640);
    if (dd)
        dd_create_basic_files(dd, (uid_t)-1L);
    free(path);
    return dd;
}

struct dump_dir *create_dump_dir_from_problem_data(problem_data_t *problem_data, const char *base_dir_name)
{
    char dir_name[sizeof("abrt-tmp-YYYY-MM-DD-HH:MM:SS-%lu") + sizeof(long)*3];
    sprintf(dir_name, "abrt-tmp-%s-%lu", iso_date_string(NULL), (long)getpid());

    struct dump_dir *dd;
    if (base_dir_name)
        dd = try_dd_create(base_dir_name, dir_name);
    else
    {
        /* Try /var/run/abrt */
        dd = try_dd_create(LOCALSTATEDIR"/run/abrt", dir_name);
        /* Try $HOME/tmp */
        if (!dd)
        {
            char *home = getenv("HOME");
            if (home && home[0])
            {
                home = concat_path_file(home, "tmp");
                /*mkdir(home, 0777); - do we want this? */
                dd = try_dd_create(home, dir_name);
                free(home);
            }
        }
//TODO: try user's home dir obtained by getpwuid(getuid())?
        /* Try /tmp */
        if (!dd)
            dd = try_dd_create("/tmp", dir_name);
    }
    if (!dd)
        return NULL;

    GHashTableIter iter;
    char *name;
    struct problem_item *value;
    g_hash_table_iter_init(&iter, problem_data);
    while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
    {
        if (name[0] == '.' || strchr(name, '/'))
        {
            error_msg("Problem data field name contains disallowed chars: '%s'", name);
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
