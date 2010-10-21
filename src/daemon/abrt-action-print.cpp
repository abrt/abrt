/*
    Write crash dump to stdout in text form.

    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009  RedHat inc.

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
#include "parse_options.h"
#include "crash_types.h"
#include "abrt_exception.h"
#include "plugin.h" /* make_description_logger */

#define PROGNAME "abrt-action-print"

static const char *dump_dir_name = ".";

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    const char *program_usage = _(
        PROGNAME" [-v] -d DIR\n"
        "\n"
        "Print information about the crash to standard output");
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Crash dump directory")),
        OPT_END()
    };

    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));

    try
    {
        struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
        if (!dd)
            return 1; /* error message is already logged */

        map_crash_data_t pCrashData;
        load_crash_data_from_debug_dump(dd, pCrashData);
        dd_close(dd);

        char *dsc = make_description_logger(pCrashData);
        fputs(dsc, stdout);
        free(dsc);
    }
    catch (CABRTException& e)
    {
        log("%s", e.what());
        return 1;
    }

    return 0;
}
