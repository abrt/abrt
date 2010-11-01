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

#define PROGNAME "abrt-action-analyze-python"

// Hash is MD5_RESULT_LEN bytes long, but we use only first 4
// (I don't know why old Python code was using only 4, I mimic that)
#define HASH_STRING_HEX_DIGITS 4

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [-vs] -d DIR\n\n"
        "Calculates and saves UUID and DUPHASH of python crash dumps"
        );
    const char *dump_dir_name = ".";
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_s = 1 << 2,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Crash dump directory")),
        OPT_BOOL(  's', NULL, NULL,                  _("Log to syslog"       )),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));

    msg_prefix = PROGNAME;
//Maybe we will want this... later
//    if (opts & OPT_s)
//    {
//        openlog(msg_prefix, 0, LOG_DAEMON);
//        logmode = LOGMODE_SYSLOG;
//    }

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;
    char *bt = dd_load_text(dd, FILENAME_BACKTRACE);

    /* Hash 1st line of backtrace and save it as UUID and DUPHASH */

    const char *bt_end = strchrnul(bt, '\n');
    unsigned char hash_bytes[MD5_RESULT_LEN];
    md5_ctx_t md5ctx;
    md5_begin(&md5ctx);
    // Better:
    // "example.py:1:<module>:ZeroDivisionError: integer division or modulo by zero"
    //md5_hash(bt_str, bt_end - bt_str, &md5ctx);
    //free(bt);
    // For now using compat version:
    {
        char *copy = xstrndup(bt, bt_end - bt);
        free(bt);
        char *s = copy;
        char *d = copy;
        unsigned colon_cnt = 0;
        while (*s && colon_cnt < 3)
        {
            if (*s != ':')
                *d++ = *s;
            else
                colon_cnt++;
            s++;
        }
        // copy = "example.py1<module>"
        md5_hash(copy, d - copy, &md5ctx);
        free(copy);
    }
    // end of compat version
    md5_end(hash_bytes, &md5ctx);

    char hash_str[HASH_STRING_HEX_DIGITS*2 + 1];
    unsigned len = HASH_STRING_HEX_DIGITS;
    char *d = hash_str;
    unsigned char *s = hash_bytes;
    while (len)
    {
        *d++ = "0123456789abcdef"[*s >> 4];
        *d++ = "0123456789abcdef"[*s & 0xf];
        s++;
        len--;
    }
    *d = '\0';

    dd_save_text(dd, CD_UUID, hash_str);
    dd_save_text(dd, FILENAME_DUPHASH, hash_str);
    dd_close(dd);

    return 0;
}
