/*
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
#include "libabrt.h"

static void trim_unstrip_output(char *result, const char *unstrip_n_output)
{
    // lines look like this:
    // 0x400000+0x209000 23c77451cf6adff77fc1f5ee2a01d75de6511dda@0x40024c - - [exe]
    // 0x400000+0x209000 ab3c8286aac6c043fd1bb1cc2a0b88ec29517d3e@0x40024c /bin/sleep /usr/lib/debug/bin/sleep.debug [exe]
    // 0x7fff313ff000+0x1000 389c7475e3d5401c55953a425a2042ef62c4c7df@0x7fff313ff2f8 . - linux-vdso.so.1
    //                ^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    // we drop everything except the marked part ^

    char *dst = result;
    const char *line = unstrip_n_output;
    while (*line)
    {
        const char *eol = strchrnul(line, '\n');
        const char *plus = (char*)memchr(line, '+', eol - line);
        if (plus)
        {
            while (++plus < eol && *plus != '@')
            {
                if (!isspace(*plus))
                {
                    *dst++ = *plus;
                }
            }
        }
        if (*eol != '\n') break;
        line = eol + 1;
    }
    *dst = '\0';
}

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
        "& [-v] -d DIR\n"
        "\n"
        "Calculates and saves UUID of coredump in problem directory DIR"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Dump directory")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    /* Run unstrip -n and trim its output, leaving only sizes and build ids */

    char *unstrip_n_output = run_unstrip_n(dump_dir_name, /*timeout_sec:*/ 30);
    if (!unstrip_n_output)
        return 1; /* bad dump_dir_name, can't run unstrip, etc... */
    /* modifies unstrip_n_output in-place: */
    trim_unstrip_output(unstrip_n_output, unstrip_n_output);

    /* Hash package + executable + unstrip_n_output and save it as UUID */

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;

    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    /* FILENAME_PACKAGE may be missing if ProcessUnpackaged = yes... */
    char *package = dd_load_text_ext(dd, FILENAME_PACKAGE, DD_FAIL_QUIETLY_ENOENT);
    /* Package variable has "firefox-3.5.6-1.fc11[.1]" format */
    /* Remove distro suffix and maybe least significant version number */
    char *p = package;
    while (*p)
    {
        if (*p == '.' && (p[1] < '0' || p[1] > '9'))
        {
            /* We found "XXXX.nondigitXXXX", trim this part */
            *p = '\0';
            break;
        }
        p++;
    }
    char *first_dot = strchr(package, '.');
    if (first_dot)
    {
        char *last_dot = strrchr(first_dot, '.');
        if (last_dot != first_dot)
        {
            /* There are more than one dot: "1.2.3"
             * Strip last part, we don't want to distinquish crashes
             * in packages which differ only by minor release number.
             */
            *last_dot = '\0';
        }
    }

    char *string_to_hash = xasprintf("%s%s%s", package, executable, unstrip_n_output);
    /*free(package);*/
    /*free(executable);*/
    /*free(unstrip_n_output);*/

    char hash_str[SHA1_RESULT_LEN*2 + 1];
    str_to_sha1str(hash_str, string_to_hash);

    dd_save_text(dd, FILENAME_UUID, hash_str);
    dd_close(dd);

    return 0;
}
