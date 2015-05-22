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
#include <client.h>

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
    return g_ptr_array_new_with_free_func((void (*)(void*)) &free_problem_data);
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
    add_to_problem_data_ext(problem_data, CD_DUMPDIR, dump_dir_name,
                            CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE + CD_FLAG_LIST);

    return problem_data;
}

static int
append_problem_data(struct dump_dir *dd, void *arg)
{
    vector_of_problem_data_t *vpd = arg;

    problem_data_t *problem_data = create_problem_data_from_dump_dir(dd);
    add_to_problem_data_ext(problem_data, CD_DUMPDIR, dd->dd_dirname,
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

void restart_as_root_if_needed(unsigned cmd_argc, const char *cmd_argv[])
{
    if (g_settings_privatereports && getuid() == 0)
        return;

    log(_("PrivateReports is enabled. Only \"root\" can see the problems detected by ABRT."));

    if (!ask_yes_no(_("Do you wan to run abrt-cli-root?")))
        return;

    int i = 0;
    char **new_args = xmalloc((cmd_argc + 2)*sizeof(char *));
    new_args[i++] = (char *)"abrt-cli-root";

    for (unsigned j = 0; j < cmd_argc; )
        new_args[i++] = (char *)cmd_argv[j++];

    new_args[i++] = (char *)NULL;

    execv(BIN_DIR"/abrt-cli-root", (char *const *)new_args);
    perror_msg(_("Could not execute abrt-gui-root via consolehelper"));
    exit(-1);
}
