/*
    Copyright (C) 2012  ABRT Team
    Copyright (C) 2012  Red Hat, Inc.

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
#include "xorg-utils.h"

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-vsoxm] [-d DIR]/[-D] [FILE]\n"
        "\n"
        "Extract Xorg crash from FILE (or standard input)"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_s = 1 << 1,
        OPT_o = 1 << 2,
        OPT_d = 1 << 3,
        OPT_D = 1 << 4,
        OPT_x = 1 << 5,
        OPT_m = 1 << 6,
    };
    char *dump_location = NULL;
    /* Keep OPT_z enums and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(  's', NULL, NULL, _("Log to syslog")),
        OPT_BOOL(  'o', NULL, NULL, _("Print found crash data on standard output")),
        OPT_STRING('d', NULL, &dump_location, "DIR", _("Create problem directory in DIR for every crash found")),
        OPT_BOOL(  'D', NULL, NULL, _("Same as -d DumpLocation, DumpLocation is specified in abrt.conf")),
        OPT_BOOL(  'x', NULL, NULL, _("Make the problem directory world readable")),
        OPT_BOOL(  'm', NULL, NULL, _("Print search string(s) to stdout and exit")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    msg_prefix = g_progname;
    if ((opts & OPT_s) || getenv("ABRT_SYSLOG"))
    {
        logmode = LOGMODE_JOURNAL;
    }

    if (opts & OPT_m)
    {
        puts("Backtrace");
        return 0;
    }

    if (opts & OPT_D)
    {
        if (opts & OPT_d)
            show_usage_and_die(program_usage_string, program_options);
        abrt_load_abrt_conf();
        dump_location = abrt_g_settings_dump_location;
        abrt_g_settings_dump_location = NULL;
        abrt_free_abrt_conf_data();
    }

    argv += optind;
    if (argv[0])
        xmove_fd(xopen(argv[0], O_RDONLY), STDIN_FILENO);

    int bt_count = 0;
    char *line = NULL;
    while ((line = xmalloc_fgetline(stdin)) != NULL)
    {
        char *p = skip_pfx(line);
        if (strcmp(p, "Backtrace:") == 0)
        {
            struct xorg_crash_info *crash_info = process_xorg_bt(&xorg_get_next_line_from_fd, stdin);
            if (crash_info)
            {
                if (opts & OPT_o)
                    xorg_crash_info_print_crash(crash_info);
                if (opts & (OPT_d|OPT_D))
                    if (bt_count++ <= ABRT_OOPS_MAX_DUMPED_COUNT)
                        xorg_crash_info_create_dump_dir(crash_info, dump_location, (opts & OPT_x));
                xorg_crash_info_free(crash_info);
            }
            else
                log_warning(_("Failed to parse Backtrace from log file"));
        }
        free(line);
    }

    return 0;
}
