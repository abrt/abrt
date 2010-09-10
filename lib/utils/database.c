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

struct db_row *db_rowcpy_from_list(GList* list)
{
    GList *first = g_list_first(list);
    struct db_row *row = (struct db_row*)xzalloc(sizeof(struct db_row));
    struct db_row *src_row = (struct db_row*)first->data;
    /* All fields are initialized below, copying is not needed
     * memcpy(row, (struct db_row*)first->data, sizeof(struct db_row));
     */

    row->db_uuid = xstrdup(src_row->db_uuid);
    row->db_uid = xstrdup(src_row->db_uid);
    row->db_inform_all = xstrdup(src_row->db_inform_all);
    row->db_dump_dir = xstrdup(src_row->db_dump_dir);
    row->db_count = xstrdup(src_row->db_count);
    row->db_reported = xstrdup(src_row->db_reported);
    row->db_message = xstrdup(src_row->db_message);
    row->db_time = xstrdup(src_row->db_time);

    return row;
}

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

