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
#ifndef PROBLEM_DATA_H_
#define PROBLEM_DATA_H_

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

struct problem_item {
    char    *content;
    unsigned flags;
};
typedef struct problem_item problem_item;

char *format_problem_item(struct problem_item *item);

/* In-memory problem data structure and accessors */

typedef GHashTable problem_data_t;

problem_data_t *new_problem_data(void);

static inline void free_problem_data(problem_data_t *problem_data)
{
    if (problem_data)
        g_hash_table_destroy(problem_data);
}

void add_to_problem_data_ext(problem_data_t *problem_data,
                const char *name,
                const char *content,
                unsigned flags);
/* Uses CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE flags */
void add_to_problem_data(problem_data_t *problem_data,
                const char *name,
                const char *content);

static inline struct problem_item *get_problem_data_item_or_NULL(problem_data_t *problem_data, const char *key)
{
    return (struct problem_item *)g_hash_table_lookup(problem_data, key);
}
const char *get_problem_item_content_or_NULL(problem_data_t *problem_data, const char *key);
/* Aborts if key is not found: */
const char *get_problem_item_content_or_die(problem_data_t *problem_data, const char *key);


/* Vector of these structures */

typedef GPtrArray vector_of_problem_data_t;

static inline problem_data_t *get_problem_data(vector_of_problem_data_t *vector, unsigned i)
{
    return (problem_data_t *)g_ptr_array_index(vector, i);
}

vector_of_problem_data_t *new_vector_of_problem_data(void);
static inline void free_vector_of_problem_data(vector_of_problem_data_t *vector)
{
    if (vector)
        g_ptr_array_free(vector, TRUE);
}


/* Conversions between in-memory and on-disk formats */

void load_problem_data_from_dump_dir(problem_data_t *problem_data, struct dump_dir *dd);
problem_data_t *create_problem_data_from_dump_dir(struct dump_dir *dd);

struct dump_dir *create_dump_dir_from_problem_data(problem_data_t *problem_data, const char *base_dir_name);

#ifdef __cplusplus
}
#endif

#endif
