/*
    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009  Red Hat, Inc.

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

#define CCPP_CONF "CCpp.conf"

static const char *dump_dir_name = ".";
/* 60 seconds was too limiting on slow machines */
static int exec_timeout_sec = 240;

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    char *i_opt = NULL;

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "& [options] -d DIR\n"
        "\n"
        "Analyzes coredump in problem directory DIR, generates and saves backtrace"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_i = 1 << 2,
        OPT_t = 1 << 3,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING( 'd', NULL, &dump_dir_name   , "DIR"           , _("Problem directory")),
        OPT_STRING( 'i', NULL, &i_opt           , "DIR1[:DIR2]...", _("Additional debuginfo directories")),
        OPT_INTEGER('t', NULL, &exec_timeout_sec,                   _("Kill gdb if it runs for more than NUM seconds")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    map_string_t *settings = new_map_string();
    if (!load_abrt_plugin_conf_file(CCPP_CONF, settings))
        error_msg("Can't load '%s'", CCPP_CONF);

    const char *value = get_map_string_item_or_NULL(settings, "DebuginfoLocation");
    char *debuginfo_location;
    if (value)
        debuginfo_location = xstrdup(value);
    else
        debuginfo_location = xstrdup(LOCALSTATEDIR"/cache/abrt-di");

    free_map_string(settings);
    char *debuginfo_dirs = NULL;
    if (i_opt)
        debuginfo_dirs = xasprintf("%s:%s", debuginfo_location, i_opt);

    /* Create gdb backtrace */
    char *backtrace = get_backtrace(dump_dir_name, exec_timeout_sec,
            (debuginfo_dirs) ? debuginfo_dirs : debuginfo_location);
    free(debuginfo_location);
    if (!backtrace)
    {
        backtrace = xstrdup("");
        log_warning("get_backtrace() returns NULL, broken core/gdb?");
    }
    free(debuginfo_dirs);
    free_abrt_conf_data();

    /* Store gdb backtrace */

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;
    dd_save_text(dd, FILENAME_BACKTRACE, backtrace);
    dd_close(dd);

    /* Don't be completely silent. gdb run takes a few seconds,
     * it is useful to let user know it (maybe) worked.
     */
    log_warning(_("Backtrace is generated and saved, %u bytes"), (int)strlen(backtrace));
    free(backtrace);

    return 0;
}
