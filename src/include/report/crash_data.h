/*
    Copyright (C) 2009  Abrt team.
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
#ifndef CRASH_DATA_H_
#define CRASH_DATA_H_

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dump_dir;

enum {
    CD_FLAG_BIN           = (1 << 0),
    CD_FLAG_TXT           = (1 << 1),
    CD_FLAG_ISEDITABLE    = (1 << 2),
    CD_FLAG_ISNOTEDITABLE = (1 << 3),
    /* Show this element in "short" info (abrt-cli -l) */
    CD_FLAG_LIST          = (1 << 4),
    CD_FLAG_UNIXTIME      = (1 << 5),
};

struct crash_item {
    char    *content;
    unsigned flags;
};
typedef struct crash_item crash_item;

char *format_crash_item(struct crash_item *item);

/* In-memory crash data structure and accessors */

typedef GHashTable crash_data_t;

crash_data_t *new_crash_data(void);

static inline void free_crash_data(crash_data_t *crash_data)
{
    if (crash_data)
        g_hash_table_destroy(crash_data);
}

void add_to_crash_data_ext(crash_data_t *crash_data,
                const char *name,
                const char *content,
                unsigned flags);
/* Uses CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE flags */
void add_to_crash_data(crash_data_t *crash_data,
                const char *name,
                const char *content);

static inline struct crash_item *get_crash_data_item_or_NULL(crash_data_t *crash_data, const char *key)
{
    return (struct crash_item *)g_hash_table_lookup(crash_data, key);
}
const char *get_crash_item_content_or_NULL(crash_data_t *crash_data, const char *key);
/* Aborts if key is not found: */
const char *get_crash_item_content_or_die(crash_data_t *crash_data, const char *key);


/* Vector of these structures */

typedef GPtrArray vector_of_crash_data_t;

static inline crash_data_t *get_crash_data(vector_of_crash_data_t *vector, unsigned i)
{
    return (crash_data_t *)g_ptr_array_index(vector, i);
}

vector_of_crash_data_t *new_vector_of_crash_data(void);
static inline void free_vector_of_crash_data(vector_of_crash_data_t *vector)
{
    if (vector)
        g_ptr_array_free(vector, TRUE);
}


/* Conversions between in-memory and on-disk formats */

void load_crash_data_from_dump_dir(crash_data_t *crash_data, struct dump_dir *dd);
crash_data_t *create_crash_data_from_dump_dir(struct dump_dir *dd);

struct dump_dir *create_dump_dir_from_crash_data(crash_data_t *crash_data, const char *base_dir_name);

#ifdef __cplusplus
}
#endif

#endif
