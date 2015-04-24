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

#define RHTS_NAME "rhtsupport.conf"
#define RHTS_USERNAME_OPTION "Login"
#define RHTS_PASSWORD_OPTION "Password"

#define UREPORT_NAME "ureport.conf"
#define UREPORT_HTTP_AUTH_OPTION "HTTPAuth"
#define UREPORT_CLIENT_AUTH_OPTION "SSLClientAuth"
#define UREPORT_RTHS_CREDENTIALS_AUTH "rhts-credentials"

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

#if AUTHENTICATED_AUTOREPORTING != 0
static int
set_ureport_http_auth(map_string_t *conf, const char *opt_value)
{
    const char *const cur_value = get_map_string_item_or_NULL(conf, UREPORT_HTTP_AUTH_OPTION);

    if (cur_value == NULL || strcmp(cur_value, opt_value) != 0)
    {
        replace_map_string_item(conf, xstrdup(UREPORT_HTTP_AUTH_OPTION), xstrdup(opt_value));
        remove_map_string_item(conf, UREPORT_CLIENT_AUTH_OPTION);

        return save_plugin_conf_file(UREPORT_NAME, conf);
    }

    /* No changes needed -> success */
    return 1;
}

static int
set_ureport_client_auth(map_string_t *conf, const char *opt_value)
{
    const char *const cur_value = get_map_string_item_or_NULL(conf, UREPORT_CLIENT_AUTH_OPTION);

    if (cur_value == NULL || strcmp(cur_value, opt_value) != 0)
    {
        replace_map_string_item(conf, xstrdup(UREPORT_CLIENT_AUTH_OPTION), xstrdup(opt_value));
        remove_map_string_item(conf, UREPORT_HTTP_AUTH_OPTION);

        return save_plugin_conf_file(UREPORT_NAME, conf);
    }

    /* No changes needed -> success */
    return 1;
}

static int
clear_ureport_auth(map_string_t *conf)
{
    const char *const http_cur_value = get_map_string_item_or_NULL(conf, UREPORT_HTTP_AUTH_OPTION);
    const char *const ssl_cur_value = get_map_string_item_or_NULL(conf, UREPORT_CLIENT_AUTH_OPTION);

    if (http_cur_value != NULL || ssl_cur_value != NULL)
    {
        remove_map_string_item(conf, UREPORT_HTTP_AUTH_OPTION);
        remove_map_string_item(conf, UREPORT_CLIENT_AUTH_OPTION);

        return save_plugin_conf_file(UREPORT_NAME, conf);
    }

    /* No changes needed -> success */
    return 1;
}

static int
set_rhts_credentials(map_string_t *conf, const char *username, const char *password)
{
    const char *const username_cur_value = get_map_string_item_or_NULL(conf, RHTS_USERNAME_OPTION);
    const char *const password_cur_value = get_map_string_item_or_NULL(conf, RHTS_PASSWORD_OPTION);

    if (  (username_cur_value == NULL || strcmp(username_cur_value, username) != 0)
       || (password_cur_value == NULL || strcmp(password_cur_value, password) != 0))
    {
        replace_map_string_item(conf, xstrdup(RHTS_USERNAME_OPTION), xstrdup(username));
        replace_map_string_item(conf, xstrdup(RHTS_PASSWORD_OPTION), xstrdup(password));

        return save_plugin_conf_file(RHTS_NAME, conf);
    }

    /* No changes needed -> success */
    return 1;
}
#endif

static const char *
get_abrt_reporting(map_string_t *conf)
{
    const char *const cur_value = get_map_string_item_or_empty(conf, OPTION_NAME);
    const int index = !!string_to_bool(cur_value);
    return REPORTING_STATES[index][0];
}

#if AUTHENTICATED_AUTOREPORTING != 0
static const char *
get_ureport_http_auth(map_string_t *conf)
{
    return get_map_string_item_or_NULL(conf, UREPORT_HTTP_AUTH_OPTION);
}

static const char *
get_ureport_client_auth(map_string_t *conf)
{
    return get_map_string_item_or_NULL(conf, UREPORT_CLIENT_AUTH_OPTION);
}
#endif

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
#if AUTHENTICATED_AUTOREPORTING != 0
    const char *program_usage_string = _(
            "& [ "STATE_MANUAL" | "STATE_AUTO" | yes | no | 1 | 0 ] \\\n"
            "  [[--anonymous] | [--username USERNAME [--password PASSWORD]] | [--certificate SOURCE]]\n"
            PROGRAM_USAGE_MIDDLE_PART
            "See abrt-auto-reporting(1), reporter-ureport(1) and reporter-rhtsupport(1)\n"
            "for more details.\n"
    );
#else
    const char *program_usage_string = _(
            "& [ "STATE_MANUAL" | "STATE_AUTO" | yes | no | 1 | 0 ]\n"
            PROGRAM_USAGE_MIDDLE_PART
            "See abrt-auto-reporting(1) and reporter-ureport(1) for more details.\n"
    );
#endif

    enum {
        OPT_v = 1 << 0,
#if AUTHENTICATED_AUTOREPORTING != 0
        OPT_a = 1 << 1,
        OPT_u = 1 << 2,
        OPT_p = 1 << 3,
        OPT_c = 1 << 4,
#endif
    };

#if AUTHENTICATED_AUTOREPORTING != 0
    int anonymous = 0;
    const char *username = NULL;
    const char *password = NULL;
    const char *certificate = NULL;
#endif

    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
#if AUTHENTICATED_AUTOREPORTING != 0
        OPT_BOOL  (  'a', "anonymous",   &anonymous,               _("Turns the authentication off")),
        OPT_STRING(  'u', "username",    &username,    "USERNAME", _("Red Hat Support user name")),
        OPT_STRING(  'p', "password",    &password,    "PASSWORD", _("Red Hat Support password, if not given, a prompt for it will be issued")),
        OPT_STRING(  'c', "certificate", &certificate, "SOURCE",   _("uReport SSL certificate paths or certificate type")),
#endif
        OPT_END()
    };

#if AUTHENTICATED_AUTOREPORTING != 0
    const unsigned opts =
#endif
    parse_opts(argc, argv, program_options, program_usage_string);

    argv += optind;
    argc -= optind;

#if AUTHENTICATED_AUTOREPORTING != 0
    if ((opts & OPT_p) && !(opts & OPT_u))
    {
        error_msg(_("You also need to specify --username for --password"));
        show_usage_and_die(program_usage_string, program_options);
    }

    if ((opts & OPT_u) && (opts & OPT_c))
    {
        error_msg(_("You can use either --username or --certificate"));
        show_usage_and_die(program_usage_string, program_options);
    }

    if ((opts & OPT_u) && (opts & OPT_a))
    {
        error_msg(_("You can use either --username or --anonymous"));
        show_usage_and_die(program_usage_string, program_options);
    }

    if ((opts & OPT_a) && (opts & OPT_c))
    {
        error_msg(_("You can use either --anonymous or --certificate"));
        show_usage_and_die(program_usage_string, program_options);
    }

#endif
    if (argc > 1)
    {
        error_msg(_("Invalid number of arguments"));
        show_usage_and_die(program_usage_string, program_options);
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
            show_usage_and_die(program_usage_string, program_options);
        }
    }

    int exit_code = EXIT_FAILURE;

    map_string_t *conf = new_map_string();
#if AUTHENTICATED_AUTOREPORTING != 0
    map_string_t *rhts_conf = new_map_string();
    map_string_t *rhts_conf_bck = NULL;
#endif
    map_string_t *ureport_conf = new_map_string();
    map_string_t *ureport_conf_bck = NULL;

    if (!load_abrt_conf_file(CONF_NAME, conf))
        goto finito;

#if AUTHENTICATED_AUTOREPORTING != 0
    if (!load_plugin_conf_file(RHTS_NAME, rhts_conf, false))
        goto finito;
#endif

    if (!load_plugin_conf_file(UREPORT_NAME, ureport_conf, false))
        goto finito;

#if AUTHENTICATED_AUTOREPORTING != 0
    if ((opts & OPT_a))
    {
        ureport_conf_bck = clone_map_string(ureport_conf);

        if (!clear_ureport_auth(ureport_conf))
            goto finito;
    }

    if ((opts & OPT_u))
    {
        char *tmp_password = NULL;
        if (!(opts & OPT_p))
        {
            password = tmp_password = ask_password(_("Password:"));
            if (tmp_password == NULL)
            {
                error_msg(_("Cannot continue without password\n"));
                goto finito;
            }
        }

        ureport_conf_bck = clone_map_string(ureport_conf);

        if (!set_ureport_http_auth(ureport_conf, UREPORT_RTHS_CREDENTIALS_AUTH))
            goto finito;

        rhts_conf_bck = clone_map_string(rhts_conf);

        if (!set_rhts_credentials(rhts_conf, username, password))
        {
            save_plugin_conf_file(UREPORT_NAME, ureport_conf_bck);
            goto finito;
        }

        free(tmp_password);
    }

    if ((opts & OPT_c))
    {
        ureport_conf_bck = clone_map_string(ureport_conf);

        if (!set_ureport_client_auth(ureport_conf, certificate))
            goto finito;
    }

#endif
    if (argc == 0)
    {
        printf("%s", get_abrt_reporting(conf));
        exit_code = EXIT_SUCCESS;

#if AUTHENTICATED_AUTOREPORTING != 0
        if (g_verbose >= 1)
        {
            const char *tmp = get_ureport_http_auth(ureport_conf);
            if (tmp != NULL)
                /* Print only the part before ':' of a string like "username:password" */
                printf(" %s (%*s)", _("HTTP Authenticated auto reporting"), (int)(strchrnul(tmp, ':') - tmp), tmp);
            else if ((tmp = get_ureport_client_auth(ureport_conf)) != NULL)
                printf(" %s (%s)", _("SSL Client Authenticated auto reporting"), tmp);
            else
                printf(" %s", _("anonymous auto reporting"));
        }
#endif
        putchar('\n');

        goto finito;
    }

    exit_code = set_abrt_reporting(conf, opt_value) ? EXIT_SUCCESS : EXIT_FAILURE;

    if (exit_code == EXIT_FAILURE)
    {
        if (ureport_conf_bck != NULL)
            save_plugin_conf_file(UREPORT_NAME, ureport_conf_bck);

#if AUTHENTICATED_AUTOREPORTING != 0
        if (rhts_conf_bck != NULL)
            save_plugin_conf_file(RHTS_NAME, rhts_conf_bck);
#endif
    }


finito:
    free_map_string(ureport_conf);
    free_map_string(ureport_conf_bck);
#if AUTHENTICATED_AUTOREPORTING != 0
    free_map_string(rhts_conf);
    free_map_string(rhts_conf_bck);
#endif
    free_map_string(conf);
    return exit_code;
}
