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

#include "libabrt.h"
#include "abrt-cli-core.h"

/* Vector of problems: */
/* problem_data_vector[i] = { "name" = { "content", CD_FLAG_foo_bits } } */

inline problem_data_t *get_problem_data(vector_of_problem_data_t *vector, unsigned i)
{
    return (problem_data_t *)g_ptr_array_index(vector, i);
}

void free_vector_of_problem_data(vector_of_problem_data_t *vector)
{
    if (vector)
        g_ptr_array_free(vector, TRUE);
}

vector_of_problem_data_t *new_vector_of_problem_data(void)
{
    return g_ptr_array_new_with_free_func((void (*)(void*)) &problem_data_free);
}

problem_data_t *fill_crash_info(const char *dump_dir_name)
{
    int sv_logmode = logmode;
    logmode = 0; /* suppress EPERM/EACCES errors in opendir */
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ DD_OPEN_READONLY);
    logmode = sv_logmode;

    if (!dd)
        return NULL;

    problem_data_t *problem_data = create_problem_data_from_dump_dir(dd);
    dd_close(dd);
    problem_data_add(problem_data, CD_DUMPDIR, dump_dir_name,
                            CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE + CD_FLAG_LIST);

    return problem_data;
}

vector_of_problem_data_t *fetch_crash_infos(GList *dir_list)
{
    vector_of_problem_data_t *ci = new_vector_of_problem_data();
    for (GList *li = dir_list; li; li = li->next)
    {
        char *dir_name = (char *)li->data;
        VERB1 log("Loading dumps from '%s'", dir_name);

        DIR *dir = opendir(dir_name);
        if (dir != NULL)
        {
            struct dirent *dent;
            while ((dent = readdir(dir)) != NULL)
            {
                if (dot_or_dotdot(dent->d_name))
                    continue; /* skip "." and ".." */

                char *dump_dir_name = concat_path_file(dir_name, dent->d_name);

                struct stat statbuf;
                if (stat(dump_dir_name, &statbuf) == 0
                    && S_ISDIR(statbuf.st_mode))
                {
                    problem_data_t *problem_data = fill_crash_info(dump_dir_name);
                    if (problem_data)
                        g_ptr_array_add(ci, problem_data);
                }
                free(dump_dir_name);
            }
            closedir(dir);
        }
    }

    return ci;
}

