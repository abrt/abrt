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

int cmd_status(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "& status [DIR]..."
        );


    bool opt_bare = false;
    int opt_since = 0;

    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_GROUP(""),
        OPT_BOOL   ('b', "bare",  &opt_bare,      _("Print only the problem count without any message")),
        OPT_INTEGER('s', "since", &opt_since,     _("Print only the problems more recent than specified timestamp")),
        OPT_END()
    };

    parse_opts(argc, (char **)argv, program_options, program_usage_string);
    argv += optind;

    GList *problem_dir_list = NULL;
    while (*argv)
        problem_dir_list = g_list_append(problem_dir_list, xstrdup(*argv++));

    unsigned int problem_count = get_problems_count(problem_dir_list, opt_since);

    /* show only if there is at least 1 problem or user set the -v */
    if (problem_count > 0 || g_verbose > 0)
    {
        if (opt_bare)
            printf("%u", problem_count);
        else
            printf(_("ABRT has detected '%u' problem(s). (For more info run: $ abrt-cli list --full)\n"), problem_count);
    }

    list_free_with_free(problem_dir_list);

    return 0;
}
