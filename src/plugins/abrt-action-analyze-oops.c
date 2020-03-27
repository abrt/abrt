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
    const char *dump_directory = ".";
    const GOptionEntry option_entries[] =
    {
        {
            "verbose",
            'v',
            G_OPTION_FLAG_NONE,
            G_OPTION_ARG_NONE, &libreport_g_verbose,
            "Be verbose",
            NULL,
        },
        {
            "directory",
            'd',
            G_OPTION_FLAG_NONE,
            G_OPTION_ARG_FILENAME, &dump_directory,
            "Problem directory",
            "DIR",
        },
        { NULL, },
    };
    g_autoptr(GOptionContext) option_context = NULL;
    g_autoptr(GError) error = NULL;
    struct dump_dir *dd;
    g_autoptr(GHashTable) settings = NULL;
    g_autofree char *oops = NULL;
    g_autofree char *hash_str = NULL;

    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    option_context = g_option_context_new (NULL);

    g_option_context_add_main_entries(option_context, option_entries, PACKAGE);
    g_option_context_set_summary(option_context,
                                 _("Calculates and saves UUID and DUPHASH for oops problem directory DIR"));
    if (!g_option_context_parse(option_context, &argc, &argv, &error))
    {
        error_msg("Parsing command-line options failed: %s", error->message);

        return EXIT_FAILURE;
    }

    libreport_export_abrt_envvars(0);

    dd = dd_opendir(dump_directory, /*flags:*/ 0);
    if (NULL == dd)
    {
        return EXIT_FAILURE;
    }
    settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    abrt_load_abrt_plugin_conf_file("oops.conf", settings);

    oops = dd_load_text(dd, FILENAME_BACKTRACE);
    hash_str = abrt_koops_hash_str(oops);
    if (NULL == hash_str)
    {
        error_msg("Can't find a meaningful backtrace for hashing in '%s'", dump_directory);

        /* Do not drop such oopses by default. */
        int drop_notreportable_oopses = 0;
        const int res = libreport_try_get_map_string_item_as_bool(settings,
                "DropNotReportableOopses", &drop_notreportable_oopses);
        if (!res || !drop_notreportable_oopses)
        {
            /* Let users know that they can configure ABRT to drop these oopses. */
            log_warning("Preserving oops '%s' because DropNotReportableOopses is 'no'", dump_directory);

            dd_save_text(dd, FILENAME_NOT_REPORTABLE,
            _("The backtrace does not contain enough meaningful function frames "
              "to be reported. It is annoying but it does not necessarily "
              "signalize a problem with your computer. ABRT will not allow "
              "you to create a report in a bug tracking system but you "
              "can contact kernel maintainers via e-mail.")
            );

            /* Try to generate the hash once more with no limits. */
            /* We need UUID file for the local duplicates look-up and DUPHASH */
            /* file is also useful because user can force ABRT to report */
            /* the oops into a bug tracking system (Bugzilla). */
            hash_str = abrt_koops_hash_str_ext(oops,
                    /* use no frame count limit */-1,
                    /* use every frame in stacktrace */0);

            /* If even this attempt fails, we can drop the oops without any hesitation. */
        }
    }
    if (NULL != hash_str)
    {
        dd_save_text(dd, FILENAME_UUID, hash_str);
        dd_save_text(dd, FILENAME_DUPHASH, hash_str);
    }

    dd_close(dd);

    if (NULL == hash_str)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
