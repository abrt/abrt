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

    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
    };

    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL('d', "delete", NULL, _("Remove PROBLEM_DIR after reporting")),
        OPT_END()
    };

    unsigned opts = parse_opts(argc, (char **)argv, program_options, program_usage_string);
    argv += optind;

    if (!argv[0])
        show_usage_and_die(program_usage_string, program_options);

    export_abrt_envvars(/*prog_prefix:*/ 0);

    load_abrt_conf();
    free_abrt_conf_data();

    while (*argv)
    {
        const char *dir_name = *argv++;

        char *free_me = NULL;
        if (access(dir_name, F_OK) != 0 && errno == ENOENT)
        {
            free_me = hash2dirname(dir_name);
            if (free_me)
                dir_name = free_me;
        }
        int status = report_problem_in_dir(dir_name,
                                             LIBREPORT_WAIT
                                           | LIBREPORT_RUN_CLI);

        /* the problem was successfully reported and option is -d */
        if((opts & OPT_d) && (status == 0 || status == EXIT_STOP_EVENT_RUN))
        {
            log(_("Deleting '%s'"), dir_name);
            delete_dump_dir_possibly_using_abrtd(dir_name);
        }

        free(free_me);

        if (status)
            exit(status);
    }

    return 0;
}
