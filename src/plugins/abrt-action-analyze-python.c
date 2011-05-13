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
#include "abrtlib.h"
#include "parse_options.h"

int main(int argc, char **argv)
{
    abrt_init(argv);

    const char *dump_dir_name = ".";

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "\b [-v] -d DIR\n"
        "\n"
        "Calculates and saves UUID and DUPHASH of python crash dumps"
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

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;
    char *bt = dd_load_text(dd, FILENAME_BACKTRACE);

    /* Hash 1st line of backtrace and save it as UUID and DUPHASH */
    /* "example.py:1:<module>:ZeroDivisionError: integer division or modulo by zero" */

    unsigned char hash_bytes[SHA1_RESULT_LEN];
    sha1_ctx_t sha1ctx;
    sha1_begin(&sha1ctx);
    const char *bt_end = strchrnul(bt, '\n');
    sha1_hash(&sha1ctx, bt, bt_end - bt);
    sha1_end(&sha1ctx, hash_bytes);
    free(bt);

    char hash_str[SHA1_RESULT_LEN*2 + 1];
    unsigned len = SHA1_RESULT_LEN;
    unsigned char *s = hash_bytes;
    char *d = hash_str;
    while (len)
    {
        *d++ = "0123456789abcdef"[*s >> 4];
        *d++ = "0123456789abcdef"[*s & 0xf];
        s++;
        len--;
    }
    *d = '\0';

    dd_save_text(dd, FILENAME_UUID, hash_str);
    dd_save_text(dd, FILENAME_DUPHASH, hash_str);
    dd_close(dd);

    return 0;
}
