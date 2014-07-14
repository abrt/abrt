/*
 * Copyright (C) 2014  ABRT team
 * Copyright (C) 2014  RedHat Inc
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
#include "oops-utils.h"

#define ABRT_JOURNAL_WATCH_STATE_FILE VAR_STATE"/abrt-dump-journal-oops.state"
#define ABRT_JOURNAL_WATCH_STATE_FILE_MODE 0600
#define ABRT_JOURNAL_WATCH_STATE_FILE_MAX_SZ (4 * 1024)

/* Limit number of buffered lines */
#define ABRT_JOURNAL_MAX_READ_LINES (1024 * 1024)

/* Forward declarations */
static void save_abrt_journal_watch_position(abrt_journal_t *journal, const char *file_name);

/*
 * Koops extractor
 */

static GList* abrt_journal_extract_kernel_oops(abrt_journal_t *journal)
{
    size_t lines_info_count = 0;
    size_t lines_info_size = 32;
    struct abrt_koops_line_info *lines_info = xmalloc(lines_info_size * sizeof(lines_info[0]));

    do
    {
        const char *line = NULL;
        if (abrt_journal_get_log_line(journal, &line) < 0)
            error_msg_and_die(_("Cannot read journal data."));

        if (lines_info_count == lines_info_size)
        {
            lines_info_size *= 2;
            lines_info = xrealloc(lines_info, lines_info_size * sizeof(lines_info[0]));
        }

        lines_info[lines_info_count].level = koops_line_skip_level(&line);
        koops_line_skip_jiffies(&line);

        lines_info[lines_info_count].ptr = xstrdup(line);

        ++lines_info_count;
    }
    while (lines_info_count < ABRT_JOURNAL_MAX_READ_LINES
            && abrt_journal_next(journal) > 0);

    GList *oops_list = NULL;
    koops_extract_oopses_from_lines(&oops_list, lines_info, lines_info_count);

    log_debug("Extracted: %d oopses", g_list_length(oops_list));

    for (size_t i = 0; i < lines_info_count; ++i)
        free(lines_info[i].ptr);

    free(lines_info);

    return oops_list;
}

/*
 * An adatapter of abrt_journal_extract_kernel_oops for abrt_journal_watch_callback
 */
struct watch_journald_settings
{
    const char *dump_location;
    int oops_utils_flags;
};

static void abrt_journal_watch_extract_kernel_oops(abrt_journal_watch_t *watch, void *data)
{
    const struct watch_journald_settings *conf = (const struct watch_journald_settings *)data;

    abrt_journal_t *journal = abrt_journal_watch_get_journal(watch);

    /* Give systemd-journal one second to suck in all kernel's strings */
    if (abrt_oops_signaled_sleep(1) > 0)
    {
        abrt_journal_watch_stop(watch);
        return;
    }

    GList *oopses = abrt_journal_extract_kernel_oops(journal);
    abrt_oops_process_list(oopses, conf->dump_location, conf->oops_utils_flags);
    g_list_free_full(oopses, (GDestroyNotify)free);

    /* Skip stuff which appeared while processing oops as it is not necessary */
    /* to catch all consecutive oopses (anyway such oopses are almost */
    /* certainly duplicates of the already extracted ones) */
    abrt_journal_seek_tail(journal);

    /* In case of disaster, lets make sure we won't read the journal messages */
    /* again. */
    save_abrt_journal_watch_position(journal, ABRT_JOURNAL_WATCH_STATE_FILE);

    if (g_abrt_oops_sleep_woke_up_on_signal > 0)
        abrt_journal_watch_stop(watch);
}

/*
 * Koops extractor end
 */

static void try_restore_abrt_journal_watch_position(abrt_journal_t *journal, const char *file_name)
{
    struct stat buf;
    if (lstat(file_name, &buf) < 0)
    {
        if (errno == ENOENT)
        {
            /* Only notice because this is expected */
            log_notice(_("Not restoring journal watch's position: file '%s' does not exist"), file_name);
            return;
        }

        perror_msg(_("Cannot restore journal watch's position form file '%s'"), file_name);
        return;
    }

    if (!(buf.st_mode & S_IFREG))
    {
        error_msg(_("Cannot restore journal watch's position: path '%s' is not regular file"), file_name);
        return;
    }

    if (buf.st_size > ABRT_JOURNAL_WATCH_STATE_FILE_MAX_SZ)
    {
        error_msg(_("Cannot restore journal watch's position: file '%s' exceeds %dB size limit"),
                file_name, ABRT_JOURNAL_WATCH_STATE_FILE_MAX_SZ);
        return;
    }

    int state_fd = open(file_name, O_RDONLY | O_NOFOLLOW);
    if (state_fd < 0)
    {
        perror_msg(_("Cannot restore journal watch's position: open('%s')"), file_name);
        return;
    }

    char *crsr = xmalloc(buf.st_size + 1);

    const int sz = full_read(state_fd, crsr, buf.st_size);
    if (sz != buf.st_size)
    {
        error_msg(_("Cannot restore journal watch's position: cannot read entire file '%s'"), file_name);
        close(state_fd);
        return;
    }

    crsr[sz] = '\0';
    close(state_fd);

    const int r = abrt_journal_set_cursor(journal, crsr);
    if (r < 0)
    {
        /* abrt_journal_set_cursor() prints error message in verbose mode */
        error_msg(_("Failed to move the journal to a cursor from file '%s'"), file_name);
        return;
    }

    free(crsr);
}

static void save_abrt_journal_watch_position(abrt_journal_t *journal, const char *file_name)
{
    char *crsr = NULL;
    const int r = abrt_journal_get_cursor(journal, &crsr);

    if (r < 0)
    {
        /* abrt_journal_set_cursor() prints error message in verbose mode */
        error_msg(_("Cannot save journal watch's position"));
        return;
    }

    int state_fd = open(file_name,
            O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW,
            ABRT_JOURNAL_WATCH_STATE_FILE_MODE);

    if (state_fd < 0)
    {
        perror_msg(_("Cannot save journal watch's position: open('%s')"), file_name);
        return;
    }

    full_write_str(state_fd, crsr);
    close(state_fd);

    free(crsr);
}

static void watch_journald(abrt_journal_t *journal, const char *dump_location, int flags)
{
    GList *koops_strings = koops_suspicious_strings_list();

    char *oops_string_filter_regex = abrt_oops_string_filter_regex();
    if (oops_string_filter_regex)
    {
        regex_t filter_re;
        if (regcomp(&filter_re, oops_string_filter_regex, REG_NOSUB) != 0)
            perror_msg_and_die(_("Failed to compile regex"));

        GList *iter = koops_strings;
        while(iter != NULL)
        {
            GList *next = g_list_next(iter);

            const int reti = regexec(&filter_re, (const char *)iter->data, 0, NULL, 0);
            if (reti == 0)
                koops_strings = g_list_delete_link(koops_strings, iter);
            else if (reti != REG_NOMATCH)
            {
                char msgbuf[100];
                regerror(reti, &filter_re, msgbuf, sizeof(msgbuf));
                error_msg_and_die("Regex match failed: %s", msgbuf);
            }

            iter = next;
        }

        regfree(&filter_re);
        free(oops_string_filter_regex);
    }

    struct watch_journald_settings watch_conf = {
        .dump_location = dump_location,
        .oops_utils_flags = flags,
    };

    struct abrt_journal_watch_notify_strings notify_strings_conf = {
        .decorated_cb = abrt_journal_watch_extract_kernel_oops,
        .decorated_cb_data = &watch_conf,
        .strings = koops_strings,
    };

    abrt_journal_watch_t *watch = NULL;
    if (abrt_journal_watch_new(&watch, journal, abrt_journal_watch_notify_strings, &notify_strings_conf) < 0)
        error_msg_and_die(_("Failed to initialize systemd-journal watch"));

    abrt_journal_watch_run_sync(watch);
    abrt_journal_watch_free(watch);

    g_list_free(koops_strings);
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
        "& [-vsoxtf] [-e]/[-c CURSOR] [-d DIR]/[-D]\n"
        "\n"
        "Extract oops from systemd-journal\n"
        "\n"
        "-c and -e options conflicts because both specifies the first read message.\n"
        "\n"
        "-e is useful only for -f because the following of journal starts by reading \n"
        "the entire journal if the last seen possition is not available.\n"
        "\n"
        "The last seen position is saved in "ABRT_JOURNAL_WATCH_STATE_FILE"\n"
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
    };

    char *cursor = NULL;
    char *dump_location = NULL;

    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(  's', NULL, NULL, _("Log to syslog")),
        OPT_BOOL(  'o', NULL, NULL, _("Print found oopses on standard output")),
        /* oopses don't contain any sensitive info, and even
         * the old koops app was showing the oopses to all users
         */
        OPT_STRING('d', NULL, &dump_location, "DIR", _("Create new problem directory in DIR for every oops found")),
        OPT_BOOL(  'D', NULL, NULL, _("Same as -d DumpLocation, DumpLocation is specified in abrt.conf")),
        OPT_BOOL(  'x', NULL, NULL, _("Make the problem directory world readable")),
        OPT_BOOL(  't', NULL, NULL, _("Throttle problem directory creation to 1 per second")),
        OPT_STRING('c', NULL, &cursor, "CURSOR", _("Start reading systemd-journal from the CURSOR position")),
        OPT_BOOL(  'e', NULL, NULL, _("Start reading systemd-journal from the end")),
        OPT_BOOL(  'f', NULL, NULL, _("Follow systemd-journal from the last seen position (if available)")),
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

    int oops_utils_flags = 0;
    if ((opts & OPT_x))
        oops_utils_flags |= ABRT_OOPS_WORLD_READABLE;

    if ((opts & OPT_t))
        oops_utils_flags |= ABRT_OOPS_THROTTLE_CREATION;

    if ((opts & OPT_o))
        oops_utils_flags |= ABRT_OOPS_PRINT_STDOUT;

    const char *const env_journal_filter = getenv("ABRT_DUMP_JOURNAL_OOPS_DEBUG_FILTER");
    static const char *kernel_journal_filter[2] = { 0 };
    kernel_journal_filter[0] = (env_journal_filter ? env_journal_filter : "SYSLOG_IDENTIFIER=kernel");
    log_debug("Using journal match: '%s'", kernel_journal_filter[0]);

    abrt_journal_t *journal = NULL;
    if (abrt_journal_new(&journal))
        error_msg_and_die(_("Cannot open systemd-journal"));

    if (abrt_journal_set_journal_filter(journal, kernel_journal_filter) < 0)
        error_msg_and_die(_("Cannot filter systemd-journal to kernel data only"));

    if ((opts & OPT_e) && abrt_journal_seek_tail(journal) < 0)
        error_msg_and_die(_("Cannot seek to the end of journal"));

    if ((opts & OPT_f))
    {
        if (!cursor)
            try_restore_abrt_journal_watch_position(journal, ABRT_JOURNAL_WATCH_STATE_FILE);
        else if(abrt_journal_set_cursor(journal, cursor))
            error_msg_and_die(_("Failed to start watch from cursor '%s'"), cursor);

        watch_journald(journal, dump_location, oops_utils_flags);

        save_abrt_journal_watch_position(journal, ABRT_JOURNAL_WATCH_STATE_FILE);
    }
    else
    {
        if (cursor && abrt_journal_set_cursor(journal, cursor))
            error_msg_and_die(_("Failed to set systemd-journal cursor '%s'"), cursor);

        /* Compatibility hack, a watch's callback gets the journal already moved
         * to a next message.*/
        abrt_journal_next(journal);

        GList *oopses = abrt_journal_extract_kernel_oops(journal);
        const int errors = abrt_oops_process_list(oopses, dump_location, oops_utils_flags);
        g_list_free_full(oopses, (GDestroyNotify)free);

        return errors;
    }

    abrt_journal_free(journal);

    return EXIT_SUCCESS;
}
