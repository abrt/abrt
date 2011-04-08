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

#define PROGNAME "abrt-action-print"

static const char *dump_dir_name = ".";
static const char *output_file = NULL;
static const char *append = "no";
static const char *open_mode = "w";

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [-v] -d DIR [-o FILE] [-a yes/no]\n"
        "\n"
        "Prints problem information to standard output or FILE"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_o = 1 << 2,
        OPT_a = 1 << 3,
        OPT_r = 1 << 4,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR"   , _("Dump directory")),
        OPT_STRING('o', NULL, &output_file  , "FILE"  , _("Output file")),
        OPT_STRING('a', NULL, &append       , "yes/no", _("Append to, or overwrite FILE")),
        OPT_BOOL(  'r', NULL, NULL          ,           _("Create reported_to in DIR")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));

    char *pfx = getenv("ABRT_PROG_PREFIX");
    if (pfx && string_to_bool(pfx))
        msg_prefix = PROGNAME;

    if (output_file)
    {
        if (string_to_bool(append))
            open_mode = "a";
        if (!freopen(output_file, open_mode, stdout))
            perror_msg_and_die("Can't open '%s'", output_file);
    }

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1; /* error message is already logged */

    crash_data_t *crash_data = create_crash_data_from_dump_dir(dd);
    dd_close(dd);

    char *dsc = make_description_logger(crash_data);
    fputs(dsc, stdout);
    free(dsc);
    free_crash_data(crash_data);

    if (output_file)
    {
        if (opts & OPT_r)
        {
            dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
            if (dd)
            {
                char *msg = xasprintf("file: %s", output_file);
                add_reported_to(dd, msg);
                free(msg);
                dd_close(dd);
            }
        }
        const char *format = (open_mode[0] == 'a' ? _("The report was appended to %s") : _("The report was stored to %s"));
        log(format, output_file);
    }

    return 0;
}
