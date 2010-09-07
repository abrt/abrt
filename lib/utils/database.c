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
#include "database.h"

void db_row_free(struct db_row *row)
{
    if (!row)
        return;

    free(row->db_uuid);
    free(row->db_uid);
    free(row->db_inform_all);
    free(row->db_dump_dir);
    free(row->db_count);
    free(row->db_reported);
    free(row->db_message);
    free(row->db_time);

    free(row);
}

void db_list_free(GList *list)
{
    if (!list)
        return;

    for (GList *li = list; li != NULL; li = g_list_next(li))
    {
        struct db_row *row = (struct db_row*)li->data;
        db_row_free(row);
    }
    g_list_free(list);
}

