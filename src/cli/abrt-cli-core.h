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

#ifndef ABRT_CLI_CORE_H_
#define ABRT_CLI_CORE_H_

#include "problem_api.h"

typedef GPtrArray vector_of_problem_data_t;

problem_data_t *get_problem_data(vector_of_problem_data_t *vector, unsigned i);

void free_vector_of_problem_data(vector_of_problem_data_t *vector);
vector_of_problem_data_t *new_vector_of_problem_data(void);
vector_of_problem_data_t *fetch_crash_infos(void);

/* Returns malloced string, or NULL if not found: */
char *find_problem_by_hash(const char *hash, GList *problems);
/* Returns malloced string, or NULL if not found: */
char *hash2dirname(const char *hash);
/* If input looks like a hash, returns malloced string, or NULL if not found.
 * Otherwise returns a copy of the input. */
char *hash2dirname_if_necessary(const char *input);


#endif /* ABRT_CLI_CORE_H_ */
