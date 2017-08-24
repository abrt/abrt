/*
 * Copyright (C) 2015  ABRT team
 * Copyright (C) 2015  RedHat Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "libabrt.h"
#include "abrt-journal.h"
#include "xorg-utils.h"
#define ABRT_JOURNAL_XORG_WATCH_STATE_FILE VAR_STATE"/abrt-dump-journal-xorg.state"
#define XORG_CONF "xorg.conf"
#define XORG_CONF_PATH "/etc/abrt/plugins/"XORG_CONF

static void
abrt_xorg_process_list_of_crashes(GList *crashes, const char *dump_location, int flags)
{
    if (crashes == NULL)
        return;

    GList *list;
    for (list = crashes; list != NULL; list = list->next)
    {
        xorg_crash_info_create_dump_dir(list->data, dump_location, (flags & ABRT_XORG_WORLD_READABLE));

        if (flags & ABRT_XORG_PRINT_STDOUT)
            xorg_crash_info_print_crash(list->data);

        if (flags & ABRT_XORG_THROTTLE_CREATION)
            if (abrt_xorg_signaled_sleep(1) > 0)
                break;
    }

    return;
}

static GList *abrt_journal_extract_xorg_crashes(abrt_journal_t *journal)
{
    GList *crash_info_list = NULL;

    do
    {
        char *line = abrt_journal_get_log_line(journal);
        if (line == NULL)
            error_msg_and_die(_("Cannot read journal data."));

        char *p = skip_pfx(line);
        if (strcmp(p, XORG_SEARCH_STRING) == 0)
        {
            struct xorg_crash_info *crash_info = process_xorg_bt(&abrt_journal_get_next_log_line, journal);
            if (crash_info)
                crash_info_list = g_list_append(crash_info_list, crash_info);
            else
                log_warning(_("Failed to parse Backtrace from journal"));
        }
        free(line);
    }
    while (abrt_journal_next(journal) > 0);

    log_warning("Found crashes: %d", g_list_length(crash_info_list));

    return crash_info_list;
}

struct watch_journald_xorg_settings
{
    const char *dump_location;
    int xorg_utils_flags;
};

static void abrt_journal_watch_extract_xorg_crashes(abrt_journal_watch_t *watch, void *data)
{
    const struct watch_journald_xorg_settings *conf = (const struct watch_journald_xorg_settings *)data;

    abrt_journal_t *journal = abrt_journal_watch_get_journal(watch);

    /* Give systemd-journal one second to suck in all crash strings */
    if (abrt_xorg_signaled_sleep(1) > 0)
    {
        abrt_journal_watch_stop(watch);
        return;
    }

    GList *crashes = abrt_journal_extract_xorg_crashes(journal);
    abrt_xorg_process_list_of_crashes(crashes, conf->dump_location, conf->xorg_utils_flags);
    g_list_free_full(crashes, (GDestroyNotify)xorg_crash_info_free);

    /* In case of disaster, lets make sure we won't read the journal messages */
    /* again. */
    abrt_journal_save_current_position(journal, ABRT_JOURNAL_XORG_WATCH_STATE_FILE);

    if (g_abrt_xorg_sleep_woke_up_on_signal > 0)
        abrt_journal_watch_stop(watch);
}

static void watch_journald(abrt_journal_t *journal, const char *dump_location, int flags)
{
    GList *xorg_strings = NULL;
    xorg_strings = g_list_prepend(xorg_strings, (gpointer)XORG_SEARCH_STRING);

    struct watch_journald_xorg_settings watch_conf = {
        .dump_location = dump_location,
        .xorg_utils_flags = flags,
    };

    struct abrt_journal_watch_notify_strings notify_strings_conf = {
        .decorated_cb = abrt_journal_watch_extract_xorg_crashes,
        .decorated_cb_data = &watch_conf,
        .strings = xorg_strings,
        .blacklisted_strings = NULL,
    };

    abrt_journal_watch_t *watch = NULL;
    if (abrt_journal_watch_new(&watch, journal, abrt_journal_watch_notify_strings, &notify_strings_conf) < 0)
        error_msg_and_die(_("Failed to initialize systemd-journal watch"));

    abrt_journal_watch_run_sync(watch);
    abrt_journal_watch_free(watch);

    g_list_free(xorg_strings);
}

int main(int argc, char *argv[])
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    /* Can't keep these strings/structs static: _() doesn't support that */

    const char *program_usage_string_template = _(
        "& [-vsoxtf] [-e]/[-c CURSOR] [-d DIR]/[-D]\n"
        "\n"
        "Extract Xorg crash from systemd-journal\n"
        "\n"
        "-c and -e options conflicts because both specifies the first read message.\n"
        "\n"
        "-e is useful only for -f because the following of journal starts by reading \n"
        "the entire journal if the last seen position is not available.\n"
        "\n"
        "The last seen position is saved in %s\n"
        "\n"
        "Journal filter is required parameter and must be specified either by parameter\n"
        "-j or in %s conf file.\n"
    );

    char program_usage_string[strlen(program_usage_string_template)
                            + strlen(ABRT_JOURNAL_XORG_WATCH_STATE_FILE)
                            + strlen(XORG_CONF_PATH)];
    sprintf(program_usage_string, program_usage_string_template,
            ABRT_JOURNAL_XORG_WATCH_STATE_FILE, XORG_CONF_PATH);

    enum {
        OPT_v = 1 << 0,
        OPT_s = 1 << 1,
        OPT_o = 1 << 2,
        OPT_d = 1 << 3,
        OPT_D = 1 << 4,
        OPT_x = 1 << 5,
        OPT_t = 1 << 6,
        OPT_c = 1 << 7,
        OPT_e = 1 << 8,
        OPT_f = 1 << 9,
        OPT_a = 1 << 10,
        OPT_J = 1 << 11,
        OPT_j = 1 << 12,
    };

    char *cursor = NULL;
    char *dump_location = NULL;
    char *journal_dir = NULL;
    GList *journal_filters = NULL;

    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(  's', NULL, NULL, _("Log to syslog")),
        OPT_BOOL(  'o', NULL, NULL, _("Print found crashes on standard output")),
        OPT_STRING('d', NULL, &dump_location, "DIR", _("Create new problem directory in DIR for every crash found")),
        OPT_BOOL(  'D', NULL, NULL, _("Same as -d DumpLocation, DumpLocation is specified in abrt.conf")),
        OPT_BOOL(  'x', NULL, NULL, _("Make the problem directory world readable")),
        OPT_BOOL(  't', NULL, NULL, _("Throttle problem directory creation to 1 per second")),
        OPT_STRING('c', NULL, &cursor, "CURSOR", _("Start reading systemd-journal from the CURSOR position")),
        OPT_BOOL(  'e', NULL, NULL, _("Start reading systemd-journal from the end")),
        OPT_BOOL(  'f', NULL, NULL, _("Follow systemd-journal from the last seen position (if available)")),
        OPT_BOOL(  'a', NULL, NULL, _("Read journal files from all machines")),
        OPT_STRING('J', NULL, &journal_dir,  "PATH", _("Read all journal files from directory at PATH")),
        OPT_LIST(  'j', NULL, &journal_filters,  "FILTER", _("Journal filter e.g. '_COMM=gdm-x-session' (may be given many times)")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

    msg_prefix = g_progname;
    if ((opts & OPT_s) || getenv("ABRT_SYSLOG"))
    {
        logmode = LOGMODE_JOURNAL;
    }

    if ((opts & OPT_c) && (opts & OPT_e))
        error_msg_and_die(_("You need to specify either -c CURSOR or -e"));

    if (opts & OPT_D)
    {
        if (opts & OPT_d)
            show_usage_and_die(program_usage_string, program_options);
        load_abrt_conf();
        dump_location = g_settings_dump_location;
        g_settings_dump_location = NULL;
        free_abrt_conf_data();
    }

    int xorg_utils_flags = 0;
    if ((opts & OPT_x))
        xorg_utils_flags |= ABRT_XORG_WORLD_READABLE;

    if ((opts & OPT_t))
        xorg_utils_flags |= ABRT_XORG_THROTTLE_CREATION;

    if ((opts & OPT_o))
        xorg_utils_flags |= ABRT_XORG_PRINT_STDOUT;

    /* get journal filters */
    const char *const env_journal_filter = getenv("ABRT_DUMP_JOURNAL_XORG_DEBUG_FILTER");
    bool free_filter_list_data = false;
    GList *xorg_journal_filter = NULL;
    if (env_journal_filter != NULL)
    {
        xorg_journal_filter = g_list_append(xorg_journal_filter, (gpointer)env_journal_filter);
        log_debug("Using journal filter from environment variable");
    }
    else if (journal_filters != NULL)
    {
        xorg_journal_filter = journal_filters;
        log_debug("Using journal filter passed by parameter -j");
    }
    else
    {
        map_string_t *settings = new_map_string();
        log_notice("Loading settings from '%s'", XORG_CONF);
        load_abrt_plugin_conf_file(XORG_CONF, settings);
        log_debug("Loaded '%s'", XORG_CONF);
        const char *conf_journal_filters = get_map_string_item_or_NULL(settings, "JournalFilters");
        xorg_journal_filter = parse_list(conf_journal_filters);
        /* list data will be free by g_list_free_full */
        free_filter_list_data = true;
        free_map_string(settings);
        if (xorg_journal_filter)
            log_debug("Using journal filter from conf file %s", XORG_CONF);
    }

    if (xorg_journal_filter == NULL)
        error_msg_and_die(_("Journal filter must be specified either by parameter -j or stored in /etc/abrt/plugins/xorg.conf file"));

    abrt_journal_t *journal = NULL;
    if ((opts & OPT_J))
    {
        log_debug("Using journal files from directory '%s'", journal_dir);

        if (abrt_journal_open_directory(&journal, journal_dir))
            error_msg_and_die(_("Cannot initialize systemd-journal in directory '%s'"), journal_dir);
    }
    else
    {
        if (((opts & OPT_a) ? abrt_journal_new_merged : abrt_journal_new)(&journal))
            error_msg_and_die(_("Cannot open systemd-journal"));
    }

    if (abrt_journal_set_journal_filter(journal, xorg_journal_filter) < 0)
        error_msg_and_die(_("Cannot filter systemd-journal to Xorg data only"));

    /* free filter list */
    if (free_filter_list_data)
        g_list_free_full(xorg_journal_filter, free);
    else
        g_list_free(xorg_journal_filter);

    if ((opts & OPT_e) && abrt_journal_seek_tail(journal) < 0)
        error_msg_and_die(_("Cannot seek to the end of journal"));

    if ((opts & OPT_f))
    {
        if (!cursor)
            abrt_journal_restore_position(journal, ABRT_JOURNAL_XORG_WATCH_STATE_FILE);
        else if(abrt_journal_set_cursor(journal, cursor))
            error_msg_and_die(_("Failed to start watch from cursor '%s'"), cursor);

        watch_journald(journal, dump_location, xorg_utils_flags);

        abrt_journal_save_current_position(journal, ABRT_JOURNAL_XORG_WATCH_STATE_FILE);
    }
    else
    {
        if (cursor && abrt_journal_set_cursor(journal, cursor))
            error_msg_and_die(_("Failed to set systemd-journal cursor '%s'"), cursor);

        /* Compatibility hack, a watch's callback gets the journal already moved
         * to a next message.*/
        abrt_journal_next(journal);

        GList *crashes = abrt_journal_extract_xorg_crashes(journal);
        abrt_xorg_process_list_of_crashes(crashes, dump_location, xorg_utils_flags);
        g_list_free_full(crashes, (GDestroyNotify)xorg_crash_info_free);
    }

    abrt_journal_free(journal);

    return EXIT_SUCCESS;
}
