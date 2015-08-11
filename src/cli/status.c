/*
    Copyright (C) 2013  ABRT Team
    Copyright (C) 2013  RedHat inc.

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

#include <unistd.h>
#include <sys/types.h>
#include "problem_api.h"
#include "abrt-cli-core.h"

static unsigned int count_problem_dirs(unsigned long since)
{
    unsigned count = 0;

    GList *problems = get_problems_over_dbus(g_cli_authenticate);
    if (problems == ERR_PTR)
        return count;

    for (GList *iter = problems; iter != NULL; iter = g_list_next(iter))
    {
        const char *problem_id = (const char *)iter->data;
        if (test_exist_over_dbus(problem_id, FILENAME_REPORTED_TO))
        {
            log_debug("Not counting problem %s: already reported", problem_id);
            continue;
        }

        char *time_str = load_text_over_dbus(problem_id, FILENAME_LAST_OCCURRENCE);
        if (time_str == ERR_PTR || time_str == NULL)
        {
            log_debug("Not counting problem %s: failed to get time element", problem_id);
            continue;
        }

        long val = atol(time_str);
        free(time_str);
        if (val < since)
        {
            log_debug("Not counting problem %s: older tham limit (%ld < %ld)", problem_id, val, since);
            continue;
        }

        count++;
    }

    return count;
}

int cmd_status(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "& status"
        );

    int opt_bare = 0; /* must be _int_, OPT_BOOL expects that! */
    int opt_since = 0;

    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL   ('b', "bare",  &opt_bare,  _("Print only the problem count without any message")),
        OPT_INTEGER('s', "since", &opt_since, _("Print only the problems more recent than specified timestamp")),
        OPT_END()
    };

    parse_opts(argc, (char **)argv, program_options, program_usage_string);

    unsigned int problem_count = count_problem_dirs(opt_since);

    /* show only if there is at least 1 problem or user set the -v */
    if (problem_count > 0 || g_verbose > 0)
    {
        if (opt_bare)
            printf("%u", problem_count);
        else
        {
            char *list_arg = opt_since ? xasprintf(" --since %d", opt_since) : xstrdup("");
            printf(_("ABRT has detected %u problem(s). For more info run: abrt-cli list%s\n"), problem_count, list_arg);
            free(list_arg);
        }
    }

    return 0;
}
