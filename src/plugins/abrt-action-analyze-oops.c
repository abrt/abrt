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
        "Calculates and saves UUID and DUPHASH for oops problem directory DIR"
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

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 1;

    map_string_t *settings = new_map_string();
    load_abrt_plugin_conf_file("oops.conf", settings);

    char *oops = dd_load_text(dd, FILENAME_BACKTRACE);
    char hash_str[SHA1_RESULT_LEN*2 + 1];
    int bad = koops_hash_str(hash_str, oops);
    if (bad)
    {
        error_msg("Can't find a meaningful backtrace for hashing in '%s'", dump_dir_name);

        /* Do not drop such oopses by default. */
        int drop_notreportable_oopses = 0;
        const int res = try_get_map_string_item_as_bool(settings,
                "DropNotReportableOopses", &drop_notreportable_oopses);
        if (!res || !drop_notreportable_oopses)
        {
            /* Let users know that they can configure ABRT to drop these oopses. */
            log_warning("Preserving oops '%s' because DropNotReportableOopses is 'no'", dump_dir_name);

            dd_save_text(dd, FILENAME_NOT_REPORTABLE,
            _("The backtrace does not contain enough meaningful function frames "
              "to be reported. It is annoying but it does not necessary "
              "signalize a problem with your computer. ABRT will not allow "
              "you to create a report in a bug tracking system but you "
              "can contact kernel maintainers via e-mail.")
            );

            /* Try to generate the hash once more with no limits. */
            /* We need UUID file for the local duplicates look-up and DUPHASH */
            /* file is also useful because user can force ABRT to report */
            /* the oops into a bug tracking system (Bugzilla). */
            bad = koops_hash_str_ext(hash_str, oops,
                    /* use no frame count limit */-1,
                    /* use every frame in stacktrace */0);

            /* If even this attempt fails, we can drop the oops without any hesitation. */
        }
    }

    free(oops);

    if (!bad)
    {
        dd_save_text(dd, FILENAME_UUID, hash_str);
        dd_save_text(dd, FILENAME_DUPHASH, hash_str);
    }

    dd_close(dd);

    free_map_string(settings);

    return bad;
}
