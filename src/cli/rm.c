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
#include "builtin-cmd.h"
#include "abrt-cli-core.h"

/* TODO npajkovs:
 *   add -n, --dry-run
 *   add -q, --quite
 */

static int remove_using_dbus(const char **dirs_strv)
{
    GList *dirs = NULL;
    while (*dirs_strv)
        dirs = g_list_prepend(dirs, (void *)*dirs_strv++);
    const int ret = delete_problem_dirs_over_dbus(dirs);
    g_list_free(dirs);
    return ret;
}

static int remove_using_abrtd_or_fs(const char **dirs_strv)
{
    int errs = 0;
    while (*dirs_strv)
    {
        int status;
        const char *rm_dir = *dirs_strv++;
        status = delete_dump_dir_possibly_using_abrtd(rm_dir);
        if (!status)
            log("rm '%s'", rm_dir);
        else
            errs++;
    }
    return errs;
}

int cmd_remove(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "& rm [options] DIR..."
        );

    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_END()
    };

    parse_opts(argc, (char **)argv, program_options, program_usage_string);
    argv += optind;

    if (!argv[0])
        show_usage_and_die(program_usage_string, program_options);

    return (g_cli_authenticate ? remove_using_dbus : remove_using_abrtd_or_fs)(argv);
}
