/*
    Copyright (C) 2014  ABRT Team
    Copyright (C) 2014  RedHat inc.

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
#include "client.h"

#include "abrt-cli-core.h"
#include "builtin-cmd.h"


enum {
    ACT_ERR = 0,
    ACT_REMOVE,
    ACT_REPORT,
    ACT_INFO,
    ACT_SKIP
};

static int process_one_crash(problem_data_t *problem_data)
{
    if (problem_data == NULL)
        return ACT_ERR;

    static const char *name_to_skip[] = {
            FILENAME_PACKAGE   ,
            FILENAME_UID       ,
            FILENAME_COUNT
    };

    char *desc = make_description(problem_data,
                        /*names_to_skip:*/ (char **)name_to_skip,
                        /*max_text_size:*/ CD_TEXT_ATT_SIZE_BZ,
                        MAKEDESC_SHOW_ONLY_LIST | MAKEDESC_SHOW_URLS);

    fputs(desc, stdout);
    free(desc);

    const char *dir_name = problem_data_get_content_or_NULL(problem_data,
                                                            CD_DUMPDIR);
    char *action = NULL;
    int ret_val = 0;
    while (ret_val == 0)
    {
        const char *not_reportable = problem_data_get_content_or_NULL(problem_data, FILENAME_NOT_REPORTABLE);

        /* if the problem is not-reportable then ask does not contain option report(e) */
        if (not_reportable != NULL)
            action = ask(_("Actions: remove(rm), info(i), skip(s):"));
        else
            action = ask(_("Actions: remove(rm), report(e), info(i), skip(s):"));

        if(strcmp(action, "rm") == 0 || strcmp(action, "remove") == 0 )
        {
            log(_("Deleting '%s'"), dir_name);
            const char *dirs_strv[] = {dir_name, NULL};
            _cmd_remove(dirs_strv);

            ret_val = ACT_REMOVE;
        }
        else if (not_reportable == NULL && (strcmp(action, "e") == 0 || strcmp(action, "report") == 0))
        {
            log(_("Reporting '%s'"), dir_name);
            const char *dirs_strv[] = {dir_name, NULL};
            _cmd_report(dirs_strv, /*do not delete*/0);

            ret_val = ACT_REPORT;
        }
        else if (strcmp(action, "i") == 0 || strcmp(action, "info") == 0)
        {
            _cmd_info(problem_data, /*detailed*/1, CD_TEXT_ATT_SIZE_BZ);

            ret_val = ACT_INFO;
        }
        else if (strcmp(action, "s") == 0 || strcmp(action, "skip") == 0)
        {
            ret_val = ACT_SKIP;
        }

        free(action);
    }

    return ret_val;
}

static void process_crashes(vector_of_problem_data_t *crash_list, long since)
{

    for (unsigned i = 0; i < crash_list->len; ++i)
    {
        problem_data_t *crash = get_problem_data(crash_list, i);

        if (since != 0)
        {
            char *s = problem_data_get_content_or_NULL(crash, FILENAME_LAST_OCCURRENCE);
            long val = s ? atol(s) : 0;
            if (val < since)
                continue;
        }

        /* do not print '\n' before first problem */
        if(i != 0)
            printf("\n");

        int action = process_one_crash(crash);

        if (i != crash_list->len - 1)
        {
            if (action == ACT_REMOVE || action == ACT_REPORT || action == ACT_INFO)
            {
                /* dummy must be free because the function ask allocate memory */
                char *dummy = ask(_("For next problem press ENTER:"));
                free(dummy);
            }
        }
    }
    return;
}

int cmd_process(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "Without --since argument, iterates over all detected problems."
    );

    int opt_since = 0;
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_INTEGER('s', "since" , &opt_since,  _("Selects only problems detected after timestamp")),
        OPT_END()
    };

    parse_opts(argc, (char **)argv, program_options, program_usage_string);

    vector_of_problem_data_t *ci = fetch_crash_infos();

    g_ptr_array_sort_with_data(ci, &cmp_problem_data, (char *) FILENAME_LAST_OCCURRENCE);

    process_crashes(ci, opt_since);

    free_vector_of_problem_data(ci);

    return 0;
}
