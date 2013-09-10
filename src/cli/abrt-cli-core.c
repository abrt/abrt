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

static int
append_problem_data(struct dump_dir *dd, void *arg)
{
    vector_of_problem_data_t *vpd = arg;

    problem_data_t *problem_data = create_problem_data_from_dump_dir(dd);
    problem_data_add(problem_data, CD_DUMPDIR, dd->dd_dirname,
                            CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE + CD_FLAG_LIST);
    g_ptr_array_add(vpd, problem_data);
    return 0;
}

vector_of_problem_data_t *fetch_crash_infos(GList *dir_list)
{
    vector_of_problem_data_t *vpd = new_vector_of_problem_data();

    for (GList *li = dir_list; li; li = li->next)
        for_each_problem_in_dir(li->data, getuid(), append_problem_data, vpd);

    return vpd;
}


static bool isxdigit_str(const char *str)
{
    do
    {
        if (*str < '0' || *str > '9')
            if ((*str|0x20) < 'a' || (*str|0x20) > 'f')
                return false;
        str++;
    } while (*str);
    return true;
}

struct name_resolution_param {
    const char *shortcut;
    unsigned strlen_shortcut;
    char *found_name;
};

static int find_dir_by_hash(struct dump_dir *dd, void *arg)
{
    struct name_resolution_param *param = arg;
    char hash_str[SHA1_RESULT_LEN*2 + 1];
    str_to_sha1str(hash_str, dd->dd_dirname);
    if (strncasecmp(param->shortcut, hash_str, param->strlen_shortcut) == 0)
    {
        if (param->found_name)
            error_msg_and_die(_("'%s' identifies more than one problem directory"), param->shortcut);
        param->found_name = xstrdup(dd->dd_dirname);
    }
    return 0;
}

char *hash2dirname(const char *hash)
{
    unsigned hash_len = strlen(hash);
    if (!isxdigit_str(hash) || hash_len < 5)
        return NULL;

    /* Try loading by dirname hash */
    struct name_resolution_param param = { hash, hash_len, NULL };
    GList *dir_list = get_problem_storages();
    for (GList *li = dir_list; li; li = li->next)
        for_each_problem_in_dir(li->data, getuid(), find_dir_by_hash, &param);
    return param.found_name;
}
