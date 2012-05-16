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

/* TODO: npajkovs
 *     add --since
 *     add --until
 *     add --pretty=oneline|raw|normal|format="%a %b %c"
 *     add  wildcard e.g. *-2011-04-01-10-* (list all problems in specific day)
 *
 * TODO?: remove base dir from list of crashes? is there a way that same crash can be in
 *       ~/.abrt/spool and /var/spool/abrt? needs more _meditation_.
 */

/** Prints basic information about a crash to stdout. */
static void print_crash(problem_data_t *problem_data, int detailed)
{
    if (!problem_data)
        return;

    char *desc;
    if (detailed)
    {
        int show_multiline = (detailed ? MAKEDESC_SHOW_MULTILINE : 0);
        desc = make_description(problem_data,
                                /*names_to_skip:*/ NULL,
                                /*max_text_size:*/ CD_TEXT_ATT_SIZE_BZ,
                                MAKEDESC_SHOW_FILES | show_multiline);
    }
    else
    {
        desc = make_description(problem_data,
                            /*names_to_skip:*/ NULL,
                            /*max_text_size:*/ CD_TEXT_ATT_SIZE_BZ,
                            MAKEDESC_SHOW_ONLY_LIST);
    }
    fprintf(stdout, "%s", desc);
    free(desc);
}

/**
 * Prints a list containing "crashes" to stdout.
 * @param include_reported
 *   Do not skip entries marked as already reported.
 */
static void print_crash_list(vector_of_problem_data_t *crash_list, int include_reported,
                             int detailed)
{
    unsigned i;
    for (i = 0; i < crash_list->len; ++i)
    {
        problem_data_t *crash = get_problem_data(crash_list, i);
        if (!include_reported)
        {
            const char *msg = get_problem_item_content_or_NULL(crash, FILENAME_REPORTED_TO);
            if (msg)
                continue;
        }

        printf("@%i\n", i);
        print_crash(crash, detailed);
        if (i != crash_list->len - 1)
            printf("\n");
    }
}

int cmd_list(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "& list [options] [DIR]..."
        );

    static int opt_full, opt_detailed;
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_GROUP(""),
        OPT_BOOL('f', "full"     , &opt_full,      _("List even reported problems")),
        /* deprecate -d option with --pretty=full*/
        OPT_BOOL('d', "detailed" , &opt_detailed,  _("Show detailed report")),
        OPT_END()
    };

    parse_opts(argc, (char **)argv, program_options, program_usage_string);
    argv += optind;

    GList *D_list = NULL;
    while (*argv)
        D_list = g_list_append(D_list, xstrdup(*argv++));
    if (!D_list)
    {
        load_abrt_conf();
        D_list = g_list_append(D_list, concat_path_file(g_get_user_cache_dir(), "abrt/spool"));
        D_list = g_list_append(D_list, xstrdup(g_settings_dump_location));
        free_abrt_conf_data();
    }

    vector_of_problem_data_t *ci = fetch_crash_infos(D_list);

    g_ptr_array_sort_with_data(ci, &cmp_problem_data, (char *) FILENAME_TIME);

    print_crash_list(ci, opt_full, opt_detailed);
    free_vector_of_problem_data(ci);
    list_free_with_free(D_list);

    return 0;
}

int cmd_info(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "& info [options] DIR..."
        );

    static int opt_detailed;
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_GROUP(""),
        /* deprecate -d option with --pretty=full*/
        OPT_BOOL('d', "detailed" , &opt_detailed,  _("Show detailed report")),
        OPT_END()
    };

    parse_opts(argc, (char **)argv, program_options, program_usage_string);
    argv += optind;

    if (!argv[0])
        show_usage_and_die(program_usage_string, program_options);

    int errs = 0;
    while (*argv)
    {
        const char *dump_dir = *argv++;
        problem_data_t *problem = fill_crash_info(dump_dir);
        if (!problem)
        {
            error_msg("no such problem directory '%s'", dump_dir);
            errs++;
            continue;
        }

        print_crash(problem, opt_detailed);
        free_problem_data(problem);
        if (*argv)
            printf("\n");
    }

    return errs;
}
