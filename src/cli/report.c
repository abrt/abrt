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

#include "abrtlib.h"
#include "builtin-cmd.h"

int cmd_report(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "\b report [options] [<dump-dir>]..."
        );

    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_END()
    };

    parse_opts(argc, (char **)argv, program_options, program_usage_string);
    if (optind < argc)
    {
        while (optind < argc)
        {
            int status = report_problem_in_dir(argv[optind++],
                                               LIBREPORT_ANALYZE
                                               | LIBREPORT_WAIT
                                               | LIBREPORT_RUN_CLI);
            if (status)
                exit(status);
        }
        exit(0);
    }

    show_usage_and_die(program_usage_string, program_options);

    return 0;
}
