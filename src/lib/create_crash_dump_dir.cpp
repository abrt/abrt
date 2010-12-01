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

using namespace std;

struct dump_dir *create_crash_dump_dir(const map_crash_data_t& crash_data)
{
    char *path = xasprintf(LOCALSTATEDIR"/run/abrt/tmp-%lu-%lu", (long)getpid(), (long)time(NULL));
    struct dump_dir *dd = dd_create(path, getuid());
    free(path);
    if (!dd)
        return NULL;

    map_crash_data_t::const_iterator its = crash_data.begin();
    while (its != crash_data.end())
    {
        const char *name, *value;
        name = its->first.c_str();

        if (name[0] == '.' || strchr(name, '/'))
        {
            error_msg("Crash data field name contains disallowed chars: '%s'", name);
            goto next;
        }

//FIXME: what to do with CD_BINs??
        /* Fields: CD_TYPE (is CD_SYS, CD_BIN or CD_TXT),
         * CD_EDITABLE, CD_CONTENT */
        if (its->second[CD_TYPE] == CD_BIN)
            goto next;

        value = its->second[CD_CONTENT].c_str();
        dd_save_text(dd, name, value);
 next:
        its++;
    }

    return dd;
}
