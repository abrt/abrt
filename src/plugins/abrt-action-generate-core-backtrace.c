/*
    Copyright (C) 2011  ABRT team
    Copyright (C) 2011  RedHat Inc

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
#include <satyr/abrt.h>
#include <satyr/utils.h>

#include "libabrt.h"

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    const char *dump_dir_name = ".";

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [-v] [-r] -d DIR\n"
        "\n"
        "Creates coredump-level backtrace from core dump and corresponding binary"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&libreport_g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Problem directory")),
        OPT_END()
    };
    /*unsigned opts =*/ libreport_parse_opts(argc, argv, program_options, program_usage_string);

    libreport_export_abrt_envvars(0);

    if (libreport_g_verbose > 1)
        sr_debug_parser = true;

    /* Let user know what's going on */
    log_notice(_("Generating core_backtrace"));

    g_autofree char *error_message = NULL;
    bool success;

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
    {
        log_warning("Can't open problem directory '%s'", dump_dir_name);
        return 1;
    }

    /* The value 240 was taken from abrt-action-generate-backtrace.c. */
    int exec_timeout_sec = 240;

    if (!dd_exist(dd, FILENAME_COREDUMP))
    {
        if (abrt_save_coredump(dd, exec_timeout_sec))
        {
            dd_delete_item(dd, FILENAME_COREDUMP);
            dd_close(dd);
            error_msg_and_die("Error: Couldn't retrieve coredump");
        }
    }
#ifdef ENABLE_NATIVE_UNWINDER

    success = sr_abrt_create_core_stacktrace(dump_dir_name, false, &error_message);
#else /* ENABLE_NATIVE_UNWINDER */

    g_autofree char *gdb_output = abrt_get_backtrace(dd, exec_timeout_sec, NULL);
    if (!gdb_output)
    {
        log_warning(_("Error: GDB did not return any data"));
        return 1;
    }

    success = sr_abrt_create_core_stacktrace_from_gdb(dump_dir_name,
                                                      gdb_output, false,
                                                      &error_message);

#endif /* ENABLE_NATIVE_UNWINDER */
    dd_close(dd);

    if (!success)
    {
        log_warning(_("Error: %s"), error_message);
        return 1;
    }

    return 0;
}
