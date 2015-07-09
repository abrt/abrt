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

int _cmd_report(const char **dirs_strv, int remove)
{
    int ret = 0;
    while (*dirs_strv)
    {
        const char *dir_name = *dirs_strv++;
        char *const real_problem_id = hash2dirname_if_necessary(dir_name);
        if (real_problem_id == NULL)
        {
            error_msg(_("Can't find problem '%s'"), dir_name);
            ++ret;
            continue;
        }

        const int res = chown_dir_over_dbus(real_problem_id);
        if (res != 0)
        {
            error_msg(_("Can't take ownership of '%s'"), real_problem_id);
            free(real_problem_id);
            ++ret;
            continue;
        }
        int status = report_problem_in_dir(real_problem_id,
                                             LIBREPORT_WAIT
                                           | LIBREPORT_RUN_CLI);

        /* the problem was successfully reported and option is -d */
        if(remove && (status == 0 || status == EXIT_STOP_EVENT_RUN))
        {
            log(_("Deleting '%s'"), real_problem_id);
            delete_dump_dir_possibly_using_abrtd(real_problem_id);
        }

        free(real_problem_id);

        if (status)
            exit(status);
    }

    return ret;
}

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

    return _cmd_report(argv, opts & OPT_d);
}
