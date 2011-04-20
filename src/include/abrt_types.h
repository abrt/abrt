/*
    Copyright (C) 2009  Denys Vlasenko (dvlasenk@redhat.com)
    Copyright (C) 2009  RedHat inc.

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
#ifndef ABRT_TYPES_H_
#define ABRT_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

/* TODO: rename to map_string_t */
typedef GHashTable map_string_h;

#define new_map_string abrt_new_map_string
map_string_h *new_map_string(void);
#define free_map_string abrt_free_map_string
void free_map_string(map_string_h *ms);
#define get_map_string_item_or_empty abrt_get_map_string_item_or_empty
const char *get_map_string_item_or_empty(map_string_h *ms, const char *key);
static inline
const char *get_map_string_item_or_NULL(map_string_h *ms, const char *key)
{
    return (const char*)g_hash_table_lookup(ms, key);
}

#ifdef __cplusplus
}
#endif

#endif
