/*
    Copyright (C) 2014  RedHat inc.

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
#include "client.h"

#include <stdio.h>

#define CONF_NAME "abrt.conf"
#define OPTION_NAME "AutoreportingEnabled"

#define STATE_MANUAL "disabled"
#define STATE_AUTO "enabled"

#define UREPORT_NAME "ureport.conf"

const char *const REPORTING_STATES[8][2] = {
    {STATE_MANUAL, "no" },
    {STATE_AUTO,   "yes"},
    {"no",         "no" },
    {"yes",        "yes"},
    {"0",          "no" },
    {"1",          "yes"},
    {"off",        "no" },
    {"on",         "yes"},
};

static int
set_abrt_reporting(GHashTable *conf, const char *opt_value)
{
    const char *const def_value = REPORTING_STATES[0][1];
    const char *const cur_value = g_hash_table_lookup(conf, OPTION_NAME);

    if (  (cur_value == NULL && strcmp(def_value, opt_value) != 0)
       || (cur_value != NULL && strcmp(cur_value, opt_value) != 0))
    {
        g_hash_table_replace(conf, g_strdup(OPTION_NAME), g_strdup(opt_value));
        return abrt_save_abrt_conf_file(CONF_NAME, conf);
    }

    /* No changes needed -> success */
    return 1;
}

static const char *
get_abrt_reporting(GHashTable *conf)
{
    const char *const cur_value = (const char *)g_hash_table_lookup(conf, OPTION_NAME);
    const int index = !!libreport_string_to_bool(cur_value ? cur_value : "");
    return REPORTING_STATES[index][0];
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");
    /* Hack:
     * Right-to-left scripts don't work properly in many terminals.
     * Hebrew speaking people say he_IL.utf8 looks so mangled
     * they prefer en_US.utf8 instead.
     */
    const char *msg_locale = setlocale(LC_MESSAGES, NULL);
    if (msg_locale && strcmp(msg_locale, "he_IL.utf8") == 0)
        setlocale(LC_MESSAGES, "en_US.utf8");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

#define PROGRAM_USAGE_MIDDLE_PART \
            "\n" \
            "Get or modify a value of the auto-reporting option. The changes will take\n" \
            "effect immediately and will be persistent.\n" \
            "\n" \
            ""STATE_MANUAL":\n" \
            "User have to report the detect problems manually\n" \
            "\n" \
            ""STATE_AUTO":\n" \
            "ABRT uploads an uReport which was generated for a detected problem\n" \
            "immediately after the detection phase. uReport generally contains a stack\n" \
            "trace which only describes the call stack of the program at the time of the\n" \
            "crash and does not contain contents of any variables.  Every uReport also\n" \
            "contains identification of the operating system, versions of the RPM packages\n" \
            "involved in the crash, and whether the program ran under a root user.\n" \
            "\n"

    abrt_init(argv);

    const char *program_usage_string = _(
            "& [ "STATE_MANUAL" | "STATE_AUTO" | yes | no | 1 | 0 ]\n"
            PROGRAM_USAGE_MIDDLE_PART
            "See abrt-auto-reporting(1) and reporter-ureport(1) for more details.\n"
    );

    enum {
        OPT_v = 1 << 0,
    };

    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&libreport_g_verbose),
        OPT_END()
    };

    libreport_parse_opts(argc, argv, program_options, program_usage_string);

    argv += optind;
    argc -= optind;

    if (argc > 1)
    {
        error_msg(_("Invalid number of arguments"));
        libreport_show_usage_and_die(program_usage_string, program_options);
    }

    const char *opt_value = NULL;
    if (argc == 1)
    {
        const char *const new_value = argv[0];
        for (int i = 0; i < sizeof(REPORTING_STATES)/sizeof(REPORTING_STATES[0]); ++i)
        {
            if (strcasecmp(new_value, REPORTING_STATES[i][0]) == 0)
            {
                opt_value = REPORTING_STATES[i][1];
                break;
            }
        }

        if (opt_value == NULL)
        {
            error_msg(_("Unknown option value: '%s'\n"), new_value);
            libreport_show_usage_and_die(program_usage_string, program_options);
        }
    }

    int exit_code = EXIT_FAILURE;

    g_autoptr(GHashTable) conf = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_autoptr(GHashTable) ureport_conf = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_autoptr(GHashTable) ureport_conf_bck = NULL;

    if (!abrt_load_abrt_conf_file(CONF_NAME, conf))
        return exit_code;

    if (!libreport_load_plugin_conf_file(UREPORT_NAME, ureport_conf, false))
        return exit_code;

    if (argc == 0)
    {
        printf("%s", get_abrt_reporting(conf));
        exit_code = EXIT_SUCCESS;

        putchar('\n');

        return exit_code;
    }

    exit_code = set_abrt_reporting(conf, opt_value) ? EXIT_SUCCESS : EXIT_FAILURE;

    if (exit_code == EXIT_FAILURE)
    {
        if (ureport_conf_bck != NULL)
            libreport_save_plugin_conf_file(UREPORT_NAME, ureport_conf_bck);

    }
    return exit_code;
}
