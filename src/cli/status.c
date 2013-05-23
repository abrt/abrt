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

struct time_range {
    unsigned count;
    unsigned long since;
};

static int count_dir_if_newer_than(struct dump_dir *dd, void *arg)
{
    struct time_range *me = arg;

    if (dd_exist(dd, FILENAME_REPORTED_TO))
        return 0;

    char *time_str = dd_load_text(dd, FILENAME_LAST_OCCURRENCE);
    long val = atol(time_str);
    free(time_str);
    if (val < me->since)
        return 0;

    me->count++;
    return 0;
}

static void count_problems_in_dir(gpointer data, gpointer arg)
{
    char *path = data;
    struct time_range *me = arg;

    VERB2 log("scanning '%s' for problems since %lu", path, me->since);

    for_each_problem_in_dir(path, getuid(), count_dir_if_newer_than, me);
}

static unsigned int count_problem_dirs(GList *paths, unsigned long since)
{
    struct time_range me;
    me.count = 0;
    me.since = since;

    g_list_foreach(paths, count_problems_in_dir, &me);

    return me.count;
}

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
    if (!problem_dir_list)
        problem_dir_list = get_problem_storages();

    unsigned int problem_count = count_problem_dirs(problem_dir_list, opt_since);

    list_free_with_free(problem_dir_list);

    /* show only if there is at least 1 problem or user set the -v */
    if (problem_count > 0 || g_verbose > 0)
    {
        if (opt_bare)
            printf("%u", problem_count);
        else
            printf(_("ABRT has detected '%u' problem(s). (For more info run: $ abrt-cli list --full)\n"), problem_count);
    }

    return 0;
}
