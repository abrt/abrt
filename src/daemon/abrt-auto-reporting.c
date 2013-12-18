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

#include <stdio.h>

#define CONF_NAME "abrt.conf"
#define OPTION_NAME "AutoreportingEnabled"

#define STATE_MANUAL "disabled"
#define STATE_AUTO "enabled"

const char *const REPORTING_STATES[6][2] = {
    {STATE_MANUAL, "no" },
    {STATE_AUTO,   "yes"},
    {"no",         "no" },
    {"yes",        "yes"},
    {"0",          "no" },
    {"1",          "yes"},
};

static int
set_abrt_reporting(map_string_t *conf, const char *opt_value)
{
    const char *const def_value = REPORTING_STATES[0][1];
    const char *const cur_value = get_map_string_item_or_NULL(conf, OPTION_NAME);

    if (  (cur_value == NULL && strcmp(def_value, opt_value) != 0)
       || (cur_value != NULL && strcmp(cur_value, opt_value) != 0))
    {
        replace_map_string_item(conf, xstrdup(OPTION_NAME), xstrdup(opt_value));
        return save_abrt_conf_file(CONF_NAME, conf);
    }

    /* No changes needed -> success */
    return 1;
}

static const char *
get_abrt_reporting(map_string_t *conf)
{
    const char *const cur_value = get_map_string_item_or_empty(conf, OPTION_NAME);
    const int index = !!string_to_bool(cur_value);
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

    abrt_init(argv);
    const char *program_usage_string = _(
            "& [ "STATE_MANUAL" | "STATE_AUTO" | yes | no | 1 | 0 ]\n"
            "\n"
            "Get or modify a value of the auto-reporting option. The changes will take\n"
            "effect immediately and will be persistent.\n"
            "\n"
            ""STATE_MANUAL":\n"
            "User have to report the detect problems manually\n"
            "\n"
            ""STATE_AUTO":\n"
            "ABRT uploads an uReport which was generated for a detected problem\n"
            "immediately after the detection phase. uReport generally contains a stack\n"
            "trace which only describes the call stack of the program at the time of the\n"
            "crash and does not contain contents of any variables.  Every uReport also\n"
            "contains identification of the operating system, versions of the RPM packages\n"
            "involved in the crash, and whether the program ran under a root user.\n"
            "\n"
            "See abrt-auto-reporting(1) for more details.\n"
    );

    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_END()
    };

    const unsigned optind = parse_opts(argc, argv, program_options, program_usage_string);

    argv += optind;
    argc -= optind;

    if (argc > 2)
    {
        error_msg(_("Invalid number of arguments"));
        show_usage_and_die(program_usage_string, program_options);
    }

    int exit_code = EXIT_FAILURE;

    map_string_t *conf = new_map_string();
    if (!load_abrt_conf_file(CONF_NAME, conf))
        goto finito;

    if (argc == 2)
    {
        const char *const new_value = argv[1];
        const char *opt_value = NULL;
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
            show_usage_and_die(program_usage_string, program_options);
        }

        exit_code = set_abrt_reporting(conf, opt_value) ? EXIT_SUCCESS : EXIT_FAILURE;
        goto finito;
    }

    printf("%s\n", get_abrt_reporting(conf));
    exit_code = EXIT_SUCCESS;

finito:
    free_map_string(conf);
    return exit_code;
}
