/*
 * Copyright (C) 2017  ABRT team
 * Copyright (C) 2017  RedHat Inc
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
#include "dockerd-utils.h"

#define ABRT_JOURNAL_DOCKERD_WATCH_STATE_FILE VAR_STATE"/abrt-dump-journal-dockerd.state"

static void
abrt_docker_container_process_list_of_crashes(GList *crashes, const char *dump_location, int flags)
{
    if (crashes == NULL)
        return;

    GList *list;
    for (list = crashes; list != NULL; list = list->next)
    {
        docker_container_crash_create_dump_dir(list->data, dump_location,
                                               (flags & ABRT_DOCKERD_WORLD_READABLE));

        if (flags & ABRT_DOCKERD_PRINT_STDOUT)
            container_crash_info_print_crash(list->data);

        if (flags & ABRT_DOCKERD_THROTTLE_CREATION)
            if (abrt_docker_container_signaled_sleep(1) > 0)
                break;
    }

    return;
}

static GList *abrt_journal_extract_docker_container_crashes(abrt_journal_t *journal)
{
    struct container_crash_info *crash_info = NULL;
    GList *crash_info_list = NULL;

    do
    {
        char *line = abrt_journal_get_log_line(journal);
        if (line == NULL)
            error_msg_and_die(_("Cannot read journal data."));

        if (0 == strncmp(line, DOCKERD_LOG_SEARCH_STRING, sizeof(DOCKERD_LOG_SEARCH_STRING) - 1))
        {
            crash_info = container_crash_info_parse_form_json_str(line);
            if (crash_info)
                crash_info_list = g_list_append(crash_info_list, crash_info);
        }
        free(line);
    }
    while (abrt_journal_next(journal) > 0);

    log_warning("Found crashes: %d", g_list_length(crash_info_list));

    return crash_info_list;
}

struct watch_journald_dockerd_settings
{
    const char *dump_location;
    int dockerd_utils_flags;
};

static void abrt_journal_watch_extract_docker_container_crashes(abrt_journal_watch_t *watch,
                                                                void *data)
{
    const struct watch_journald_dockerd_settings *conf = (const struct watch_journald_dockerd_settings *)data;

    abrt_journal_t *journal = abrt_journal_watch_get_journal(watch);

    /* Give systemd-journal one second to suck in all crash strings */
    if (abrt_docker_container_signaled_sleep(1) > 0)
    {
        abrt_journal_watch_stop(watch);
        return;
    }

    GList *crashes = abrt_journal_extract_docker_container_crashes(journal);
    abrt_docker_container_process_list_of_crashes(crashes, conf->dump_location, conf->dockerd_utils_flags);
    g_list_free_full(crashes, (GDestroyNotify)container_crash_info_free);

    /* In case of disaster, lets make sure we won't read the journal messages */
    /* again. */
    abrt_journal_save_current_position(journal, ABRT_JOURNAL_DOCKERD_WATCH_STATE_FILE);

    if (g_abrt_docker_container_sleep_woke_up_on_signal > 0)
        abrt_journal_watch_stop(watch);
}

static void watch_journald(abrt_journal_t *journal, const char *dump_location, int flags)
{
    GList *dockerd_log_strings = NULL;
    dockerd_log_strings = g_list_prepend(dockerd_log_strings, (gpointer)DOCKERD_LOG_SEARCH_STRING);

    struct watch_journald_dockerd_settings watch_conf = {
        .dump_location = dump_location,
        .dockerd_utils_flags = flags,
    };

    struct abrt_journal_watch_notify_strings notify_strings_conf = {
        .decorated_cb = abrt_journal_watch_extract_docker_container_crashes,
        .decorated_cb_data = &watch_conf,
        .strings = dockerd_log_strings,
        .blacklisted_strings = NULL,
    };

    abrt_journal_watch_t *watch = NULL;
    if (abrt_journal_watch_new(&watch, journal, abrt_journal_watch_notify_strings, &notify_strings_conf) < 0)
        error_msg_and_die(_("Failed to initialize systemd-journal watch"));

    abrt_journal_watch_run_sync(watch);
    abrt_journal_watch_free(watch);

    g_list_free(dockerd_log_strings);
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
    const char *program_usage_string = _(
        "& [-vsf] [-e]/[-c CURSOR] [-t INT]/[-T] [-d DIR]/[-D]\n"
        "\n"
        "Extract docker container problems from systemd-journal\n"
        "\n"
        "-c and -e options conflicts because both specifies the first read message.\n"
        "\n"
        "-e is useful only for -f because the following of journal starts by reading \n"
        "the entire journal if the last seen possition is not available.\n"
        "\n"
        "The last seen position is saved in "ABRT_JOURNAL_DOCKERD_WATCH_STATE_FILE"\n"
    );
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
    };

    char *cursor = NULL;
    char *dump_location = NULL;
    char *journal_dir = NULL;

    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(  's', NULL, NULL, _("Log to syslog")),
        OPT_BOOL(  'o', NULL, NULL, _("Print found crashes on standard output")),
        OPT_STRING('d', NULL, &dump_location, "DIR", _("Create new problem directory in DIR for every container problem")),
        OPT_BOOL(  'D', NULL, NULL, _("Same as -d DumpLocation, DumpLocation is specified in abrt.conf")),
        OPT_BOOL(  'x', NULL, NULL, _("Make the problem directory world readable")),
        OPT_BOOL(  't', NULL, NULL, _("Throttle problem directory creation to 1 per second")),
        OPT_STRING('c', NULL, &cursor, "CURSOR", _("Start reading systemd-journal from the CURSOR position")),
        OPT_BOOL(  'e', NULL, NULL, _("Start reading systemd-journal from the end")),
        OPT_BOOL(  'f', NULL, NULL, _("Follow systemd-journal from the last seen position (if available)")),
        OPT_BOOL(  'a', NULL, NULL, _("Read journal files from all machines")),
        OPT_STRING('J', NULL, &journal_dir,  "PATH", _("Read all journal files from directory at PATH")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);

    export_abrt_envvars(0);

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

    int dockerd_utils_flags = 0;
    if ((opts & OPT_x))
        dockerd_utils_flags |= ABRT_DOCKERD_WORLD_READABLE;

    if ((opts & OPT_t))
        dockerd_utils_flags |= ABRT_DOCKERD_THROTTLE_CREATION;

    if ((opts & OPT_o))
        dockerd_utils_flags |= ABRT_DOCKERD_PRINT_STDOUT;

    const char *const env_journal_filter = getenv("ABRT_DUMP_JOURNAL_DOCKERD_DEBUG_FILTER");
    GList *dockerd_journal_filter = NULL;
    dockerd_journal_filter = g_list_append(dockerd_journal_filter,
           (env_journal_filter ? (gpointer)env_journal_filter : (gpointer)"_COMM=dockerd-current"));

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

    if (abrt_journal_set_journal_filter(journal, dockerd_journal_filter) < 0)
        error_msg_and_die(_("Cannot filter systemd-journal to systemd-dockerd data only"));

    g_list_free(dockerd_journal_filter);

    if ((opts & OPT_e) && abrt_journal_seek_tail(journal) < 0)
        error_msg_and_die(_("Cannot seek to the end of journal"));

    if ((opts & OPT_f))
    {
        if (!cursor)
            abrt_journal_restore_position(journal, ABRT_JOURNAL_DOCKERD_WATCH_STATE_FILE);
        else if(abrt_journal_set_cursor(journal, cursor))
            error_msg_and_die(_("Failed to start watch from cursor '%s'"), cursor);

        watch_journald(journal, dump_location, dockerd_utils_flags);

        abrt_journal_save_current_position(journal, ABRT_JOURNAL_DOCKERD_WATCH_STATE_FILE);
    }
    else
    {
        if (cursor && abrt_journal_set_cursor(journal, cursor))
            error_msg_and_die(_("Failed to set systemd-journal cursor '%s'"), cursor);

        /* Compatibility hack, a watch's callback gets the journal already moved
         * to a next message.*/
        abrt_journal_next(journal);

        GList *crashes = abrt_journal_extract_docker_container_crashes(journal);
        abrt_docker_container_process_list_of_crashes(crashes, dump_location, dockerd_utils_flags);
        g_list_free_full(crashes, (GDestroyNotify)container_crash_info_free);
    }

    abrt_journal_free(journal);

    return EXIT_SUCCESS;
}
