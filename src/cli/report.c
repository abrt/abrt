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
#include "builtin-cmd.h"

int cmd_report(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "& report [options] DIR..."
        );

    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_END()
    };

    parse_opts(argc, (char **)argv, program_options, program_usage_string);
    argv += optind;

    if (!argv[0])
        show_usage_and_die(program_usage_string, program_options);

    load_abrt_conf();
    char *home = getenv("HOME");
    GList *D_list = NULL;
    if (home)
        D_list = g_list_append(D_list, concat_path_file(home, ".abrt/spool"));
    D_list = g_list_append(D_list, xstrdup(g_settings_dump_location));
    free_abrt_conf_data();

    while (*argv)
    {
        const char *dir_name = *argv++;

        vector_of_problem_data_t *ci = NULL;
        if (*dir_name == '@')
        {
            dir_name++;
            unsigned at = xatoi_positive(dir_name);

            ci = fetch_crash_infos(D_list);
            if (at >= ci->len)
                error_msg_and_die("error: number is out of range '%s'", dir_name);

            g_ptr_array_sort_with_data(ci, &cmp_problem_data, (char *) FILENAME_LAST_OCCURRENCE);
            problem_data_t *pd = get_problem_data(ci, at);

            dir_name = get_problem_item_content_or_NULL(pd, CD_DUMPDIR);
        }

        int status = report_problem_in_dir(dir_name,
                                           LIBREPORT_ANALYZE
                                           | LIBREPORT_WAIT
                                           | LIBREPORT_RUN_CLI);
        free_vector_of_problem_data(ci);
        if (status)
            exit(status);
    }

    return 0;
}
