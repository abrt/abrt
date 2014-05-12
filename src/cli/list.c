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
                            MAKEDESC_SHOW_ONLY_LIST | MAKEDESC_SHOW_URLS);
    }
    fprintf(stdout, "%s", desc);
    free(desc);
}

/**
 * Prints a list containing "crashes" to stdout.
 * @param only_unreported
 *   Do not skip entries marked as already reported.
 */
static bool print_crash_list(vector_of_problem_data_t *crash_list, int detailed, int only_not_reported, long since, long until)
{
    bool output = false;
    unsigned i;
    for (i = 0; i < crash_list->len; ++i)
    {
        problem_data_t *crash = get_problem_data(crash_list, i);
        if (only_not_reported)
        {
            if (!get_problem_item_content_or_NULL(crash, FILENAME_REPORTED_TO))
                continue;
        }
        if (since || until)
        {
            const char *s = get_problem_item_content_or_NULL(crash, FILENAME_LAST_OCCURRENCE);
            long val = s ? atol(s) : 0;
            if (since && val < since)
                continue;
            if (until && val > until)
                continue;
        }

        printf("@%i\n", i);
        print_crash(crash, detailed);
        if (i != crash_list->len - 1)
            printf("\n");
        output = true;
    }
    return output;
}

int cmd_list(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "& list [options] [DIR]..."
        );

    int opt_not_reported = 0;
    int opt_detailed = 0;
    int opt_since = 0;
    int opt_until = 0;
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_GROUP(""),
        OPT_BOOL('n', "not-reported"     , &opt_not_reported,      _("List only not-reported problems")),
        /* deprecate -d option with --pretty=full*/
        OPT_BOOL('d', "detailed" , &opt_detailed,  _("Show detailed report")),
        OPT_INTEGER('s', "since" , &opt_since,  _("List only the problems more recent than specified timestamp")),
        OPT_INTEGER('u', "until" , &opt_until,  _("List only the problems older than specified timestamp")),
        OPT_END()
    };

    parse_opts(argc, (char **)argv, program_options, program_usage_string);
    argv += optind;

    GList *D_list = NULL;
    while (*argv)
        D_list = g_list_append(D_list, xstrdup(*argv++));
    if (!D_list)
        D_list = get_problem_storages();

    vector_of_problem_data_t *ci = fetch_crash_infos(D_list);

    g_ptr_array_sort_with_data(ci, &cmp_problem_data, (char *) FILENAME_LAST_OCCURRENCE);

    const bool output = print_crash_list(ci, opt_detailed, opt_not_reported, opt_since, opt_until);

    free_vector_of_problem_data(ci);
    list_free_with_free(D_list);

    load_abrt_conf();
    if (!g_settings_autoreporting)
    {
        if (output)
            putchar('\n');

        printf(_("The Autoreporting feature is disabled. Please consider enabling it by issuing\n"
                 "'abrt-auto-reporting enabled' as a user with root privileges\n"));
    }

    return 0;
}

int cmd_info(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "& info [options] DIR..."
        );

    int opt_detailed = 0;
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
