/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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

#include <satyr/stacktrace.h>
#include <satyr/python/stacktrace.h>
#include <satyr/thread.h>
#include <satyr/python/frame.h>
#include <satyr/frame.h>

int main(int argc, char **argv)
{
    g_autofree char *checksum = NULL;

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
        "& [-v] -d DIR\n"
        "\n"
        "Calculates and saves UUID and DUPHASH of python crash dumps"
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

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;
    char *bt = dd_load_text(dd, FILENAME_BACKTRACE);

    /* save crash_function and exception_name into dumpdir */
    char *error_message = NULL;
    struct sr_stacktrace *stacktrace = sr_stacktrace_parse(SR_REPORT_PYTHON,
                                                           (const char *)bt, &error_message);
    if (stacktrace)
    {
        struct sr_python_stacktrace *python_stacktrace = (struct sr_python_stacktrace *)stacktrace;
        if (python_stacktrace->exception_name)
            dd_save_text(dd, FILENAME_EXCEPTION_TYPE, python_stacktrace->exception_name);
        /* thread is the same as stacktrace, if stacktrace is not NULL, thread
         * is not NULL as well */
        struct sr_thread *thread = sr_stacktrace_find_crash_thread(stacktrace);
        struct sr_python_frame *frame = (struct sr_python_frame *)sr_thread_frames(thread);
        if (frame && frame->function_name)
            dd_save_text(dd, FILENAME_CRASH_FUNCTION, frame->function_name);

        sr_stacktrace_free(stacktrace);
    }
    else
    {
        error_msg("Can't parse stacktrace: %s", error_message);
        free(error_message);
    }

    /* Hash 1st line of backtrace and save it as UUID and DUPHASH */
    /* "example.py:1:<module>:ZeroDivisionError: integer division or modulo by zero" */

    char *bt_end = strchrnul(bt, '\n');
    *bt_end = '\0';
    checksum = g_compute_checksum_for_string(G_CHECKSUM_SHA1, bt, -1);

    free(bt);

    dd_save_text(dd, FILENAME_UUID, checksum);
    dd_save_text(dd, FILENAME_DUPHASH, checksum);
    dd_close(dd);

    return 0;
}
