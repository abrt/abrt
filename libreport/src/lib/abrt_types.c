/*
    Copyright (C) 2010  ABRT Team
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

map_string_h *new_map_string(void)
{
    return g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
}

void free_map_string(map_string_h *ms)
{
    if (ms)
        g_hash_table_destroy(ms);
}

const char *get_map_string_item_or_empty(map_string_h *ms, const char *key)
{
    const char *v = (const char*)g_hash_table_lookup(ms, key);
    if (!v) v = "";
    return v;
}
