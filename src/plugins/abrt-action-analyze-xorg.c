/*
    Copyright (C) 2012  ABRT team
    Copyright (C) 2012  RedHat Inc

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
#include <paths.h>
#include "libabrt.h"

static
void trim_spaces(char *str)
{
    char *src = str;
    char *dst = str;
    while (*src)
    {
        if (!isspace(*src))
            *dst++ = *src;
        src++;
    }
    *dst = '\0';
}

static
char* is_in_comma_separated_list_with_fmt(const char *value, const char *fmt, const char *list)
{
    if (!list)
        return false;
    while (*list)
    {
        const char *comma = strchrnul(list, ',');
        char *pattern = xasprintf(fmt, (int)(comma - list), list);
        char *match = strstr(value, pattern);
        free(pattern);
        if (match)
            return xstrndup(list, comma - list);
        if (!*comma)
            break;
        list = comma + 1;
    }
    return NULL;
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
        "Calculates and saves UUID and DUPHASH for xorg problem directory DIR"
        );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR", _("Problem directory")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    map_string_t *settings = new_map_string();
    VERB1 log("Loading settings from '%s'", "/etc/abrt/xorg.conf");
    load_conf_file("/etc/abrt/xorg.conf", settings, /*skip key w/o values:*/ false);
    VERB3 log("Loaded '%s'", "/etc/abrt/xorg.conf");
    char *BlacklistedXorgModules = xstrdup(get_map_string_item_or_empty(settings, "BlacklistedXorgModules"));
    trim_spaces(BlacklistedXorgModules);
    free_map_string(settings);

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;

    char *backtrace = dd_load_text(dd, FILENAME_BACKTRACE);
    char *xorg_log = dd_load_text_ext(dd, "Xorg.0.log", DD_FAIL_QUIETLY_ENOENT);
    char *blacklisted = is_in_comma_separated_list_with_fmt(backtrace, "/%.*s", BlacklistedXorgModules);
    if (!blacklisted)
        blacklisted = is_in_comma_separated_list_with_fmt(xorg_log, "LoadModule: \"%.*s\"", BlacklistedXorgModules);
    free(backtrace);
    free(xorg_log);

    if (blacklisted)
    {
        char *foobared = xasprintf(_("Module '%s' was loaded - won't report this crash"), blacklisted);
        free(blacklisted);
        dd_save_text(dd, FILENAME_NOT_REPORTABLE, foobared);
        free(foobared);
        dd_close(dd);
        return 0;
    }

    dd_close(dd);

    xchdir(dump_dir_name);

    /* Get ready for extremely ugly sight.
     *
     * # Generate duplicate detection hashes.
     * # To err on the "flag it as a dup" side is way better than the opposite.
     * # To this end:
     * # - sanitize whitespace
     * # - remove N: prefix
     * # - remove path: we don't care whether it's /usr/lib64/foo or /lib/foo
     * # - remove VERSION from so.VERSION
     * # - drop main() invocation
     * # - replace all hex constants with string "0xZ".
     * # - drop adjacent duplicate lines
     */
    execlp(_PATH_BSHELL, _PATH_BSHELL, "-c",
        "sed \\"
    "\n""        -e 's/[ \\t][ \\t]*/ /g' -e 's/  *$//' \\"
    "\n""        -e 's/^[0-9][0-9]*: //' \\"
    "\n""        -e 's@^/[^ ]*/@@' \\"
    "\n""        -e 's/\\.so\\.[0-9][0-9]*/.so/' \\"
    "\n""        -e '/libc_start_main/d' \\"
    "\n""        -e 's/0x[0-9a-fA-F][0-9a-fA-F]*/0xZ/g' \\"
    "\n""        backtrace \\"
    "\n""| uniq \\"
    "\n""| sha1sum | sed 's/[ \\t].*//' >uuid"
    "\n""test -f uuid && cp uuid duphash",
    NULL);
    perror_msg_and_die("Can't execute '%s'", _PATH_BSHELL);
}
