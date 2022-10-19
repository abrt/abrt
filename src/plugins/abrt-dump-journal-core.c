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

#define ABRT_JOURNAL_WATCH_STATE_FILE VAR_STATE"/abrt-dump-journal-core.state"

enum {
    ABRT_CORE_PRINT_STDOUT = 1 << 0,
};

/*
 * A journal message is a set of key value pairs in the following format:
 *   FIELD_NAME=${binary data}
 *
 * A journal message contains many fields useful in syslog but ABRT doesn't
 * need all of them. So the following list defines mapping between journal
 * fields and ABRT problem items.
 *
 * ABRT goes through the list and for each item reads journal field called
 * 'item.name' and saves its contents in $DUMP_DIRECTORY/'item.file'.
 */
struct field_mapping {
    const char *name;
    const char *file;
} fields [] = {
    { .name = "COREDUMP_EXE",               .file = FILENAME_EXECUTABLE, },
    { .name = "COREDUMP_CMDLINE",           .file = FILENAME_CMDLINE, },
    { .name = "COREDUMP_PROC_STATUS",       .file = FILENAME_PROC_PID_STATUS, },
    { .name = "COREDUMP_PROC_MAPS",         .file = FILENAME_MAPS, },
    { .name = "COREDUMP_PROC_LIMITS",       .file = FILENAME_LIMITS, },
    { .name = "COREDUMP_PROC_CGROUP",       .file = FILENAME_CGROUP, },
    { .name = "COREDUMP_ENVIRON",           .file = FILENAME_ENVIRON, },
    { .name = "COREDUMP_CWD",               .file = FILENAME_PWD, },
    { .name = "COREDUMP_ROOT",              .file = FILENAME_ROOTDIR, },
    { .name = "COREDUMP_OPEN_FDS",          .file = FILENAME_OPEN_FDS, },
    { .name = "COREDUMP_UID",               .file = FILENAME_UID, },
    //{ .name = "COREDUMP_GID",               .file = FILENAME_GID, },
    { .name = "COREDUMP_PID",               .file = FILENAME_PID, },
    { .name = "COREDUMP_PROC_MOUNTINFO",    .file = FILENAME_MOUNTINFO, },
};

/*
 * Something like 'struct problem_data' but optimized for copying data from
 * journald to ABRT.
 *
 * 'struct problem_data' allocates a new memory for every single item and I
 * found that very inefficient in this case.
 *
 * The following structure holds data that we already retreived from journald
 * so we won't need to retrieve the data again.
 *
 * Why we retrieve data before we store them? Because we do some checking
 * before we start saving data in ABRT. We check whether the signal is one of
 * those we are interested in or whether the executable crashes too often to
 * ignore the current crash ...
 */
struct crash_info
{
    abrt_journal_t *ci_journal;

    int ci_signal_no;
    const char *ci_signal_name;
    char *ci_executable_path;          ///< /full/path/to/executable
    const char *ci_executable_name;    ///< executable
    uid_t ci_uid;
    pid_t ci_pid;

    struct field_mapping *ci_mapping;
    size_t ci_mapping_items;
};

/*
 * ABRT watch core configuration
 */
typedef struct
{
    const char *awc_dump_location;
    int awc_throttle;
    int awc_run_flags;
}
abrt_watch_core_conf_t;


/*
 * A helper structured holding executable name and its last occurrence time.
 *
 * It is rather an array than a queue. Ii uses statically allocated array and
 * the head member points the next position for creating a new entry (the next
 * position may be already occupied and in such case the data shall be released).
 */
struct occurrence_queue
{
    int oq_head;       ///< the first empty index
    unsigned oq_size;  ///< size of the queue

    struct last_occurrence
    {
        time_t oqlc_stamp;
        char *oqlc_executable;
    } oq_occurrences[8];

} s_queue = {
    .oq_head = -1,
    .oq_size = 8,
};

static time_t
abrt_journal_get_last_occurrence(const char *executable)
{
    if (s_queue.oq_head < 0)
        return 0;

    unsigned index = s_queue.oq_head == 0 ? s_queue.oq_size - 1 : s_queue.oq_head - 1;
    for (unsigned i = 0; i < s_queue.oq_size; ++i)
    {
        if (s_queue.oq_occurrences[index].oqlc_executable == NULL)
            break;

        if (strcmp(executable, s_queue.oq_occurrences[index].oqlc_executable) == 0)
            return s_queue.oq_occurrences[index].oqlc_stamp;

        if (index-- == 0)
            index = s_queue.oq_size - 1;
    }

    return 0;
}

static void
abrt_journal_update_occurrence(const char *executable, time_t ts)
{
    if (s_queue.oq_head < 0)
        s_queue.oq_head = 0;
    else
    {
        unsigned index = s_queue.oq_head == 0 ? s_queue.oq_size - 1 : s_queue.oq_head - 1;
        for (unsigned i = 0; i < s_queue.oq_size; ++i)
        {
            if (s_queue.oq_occurrences[index].oqlc_executable == NULL)
                break;

            if (strcmp(executable, s_queue.oq_occurrences[index].oqlc_executable) == 0)
            {
                /* Enhancement: move this entry right behind head */
                s_queue.oq_occurrences[index].oqlc_stamp = ts;
                return;
            }

            if (index-- == 0)
                index = s_queue.oq_size - 1;
        }
    }

    s_queue.oq_occurrences[s_queue.oq_head].oqlc_stamp = ts;
    free(s_queue.oq_occurrences[s_queue.oq_head].oqlc_executable);
    s_queue.oq_occurrences[s_queue.oq_head].oqlc_executable = g_strdup(executable);

    if (++s_queue.oq_head >= s_queue.oq_size)
        s_queue.oq_head = 0;

    return;
}

/*
 * Converts a journal message into an intermediate ABRT problem (struct crash_info).
 *
 * Refuses to create the problem in the following cases:
 * - the crashed executable has 'abrt' prefix
 * - the signals is not fatal (see signal_is_fatal())
 * - the journal message misses one of the following fields
 *   - COREDUMP_SIGNAL
 *   - COREDUMP_EXE
 *   - COREDUMP_UID
 *   - COREDUMP_PROC_STATUS
 * - if any data does not have an expected format
 */
static int
abrt_journal_core_retrieve_information(abrt_journal_t *journal, struct crash_info *info)
{
    if (!abrt_journal_get_int(journal, "COREDUMP_SIGNAL", &info->ci_signal_no) != 0)
    {
        log_info("Failed to get signal number from journal message");
        return -EINVAL;
    }

    if (!signal_is_fatal(info->ci_signal_no, &(info->ci_signal_name)))
    {
        log_info("Signal '%d' is not fatal: ignoring crash", info->ci_signal_no);
        return 1;
    }

    info->ci_executable_path = abrt_journal_get_string_field(journal, "COREDUMP_EXE", NULL);
    if (info->ci_executable_path == NULL)
    {
        log_notice("Could not get crashed 'executable'.");
        return -ENOENT;
    }

    info->ci_executable_name = strrchr(info->ci_executable_path, '/');
    if (info->ci_executable_name == NULL)
    {
        info->ci_executable_name = info->ci_executable_path;
    }
    else if(strncmp(++(info->ci_executable_name), "abrt", 4) == 0)
    {
        error_msg("Ignoring crash of ABRT executable '%s'", info->ci_executable_path);
        return 1;
    }

    if (!abrt_journal_get_uid(journal, "COREDUMP_UID", &info->ci_uid))
    {
        log_info("Failed to get UID from journal message");
        return -EINVAL;
    }

    /* This is not fatal, the pid is used only in dumpdir name */
    if (!abrt_journal_get_pid(journal, "COREDUMP_PID", &info->ci_pid))
    {
        log_notice("Failed to get PID from journal message.");
        info->ci_pid = getpid();
    }

    char *proc_status = abrt_journal_get_string_field(journal, "COREDUMP_PROC_STATUS", NULL);
    if (proc_status == NULL)
    {
        log_info("Failed to get /proc/[pid]/status from journal message");
        return -ENOENT;
    }

    int tmp_fsuid = libreport_get_fsuid(proc_status);
    if (tmp_fsuid < 0)
        return -EINVAL;

    if ((uid_t)tmp_fsuid != info->ci_uid)
    {
        /* use root for suided apps unless it's explicitly set to UNSAFE */
        info->ci_uid = (dump_suid_policy() != DUMP_SUID_UNSAFE) ? 0 : tmp_fsuid;
    }

    return 0;
}

/*
 * Initializes ABRT problem directory and save the relevant journal message
 * fileds in that directory.
 */
static int
save_systemd_coredump_in_dump_directory(struct dump_dir *dd, struct crash_info *info)
{
    char coredump_path[PATH_MAX + 1] = { '\0' };
    if (coredump_path != abrt_journal_get_string_field(info->ci_journal, "COREDUMP_FILENAME", coredump_path))
        log_debug("Processing coredumpctl entry without a real file");

    if (g_str_has_suffix(coredump_path, ".lz4") ||
        g_str_has_suffix(coredump_path, ".xz") ||
        g_str_has_suffix(coredump_path, ".zst"))
    {
        if (dd_copy_file_unpack(dd, FILENAME_COREDUMP, coredump_path))
            return -1;
    }
    else if (strlen(coredump_path) > 0)
    {
        if (dd_copy_file(dd, FILENAME_COREDUMP, coredump_path))
            return -1;
    }
    else
    {
        const char *data = NULL;
        size_t data_len = 0;
        int r = abrt_journal_get_field(info->ci_journal, "COREDUMP", (const void **)&data, &data_len);
        if (r < 0)
        {
            log_info("Ignoring coredumpctl entry without core dump file.");
            return -1;
        }

        dd_save_binary(dd, FILENAME_COREDUMP, data, data_len);
    }

    dd_save_text(dd, FILENAME_ABRT_VERSION, VERSION);
    dd_save_text(dd, FILENAME_TYPE, "CCpp");
    dd_save_text(dd, FILENAME_ANALYZER, "abrt-journal-core");

    g_autofree char *reason = NULL;
    if (info->ci_signal_name == NULL)
        reason = g_strdup_printf("%s killed by signal %d", info->ci_executable_name, info->ci_signal_no);
    else
        reason = g_strdup_printf("%s killed by SIG%s", info->ci_executable_name, info->ci_signal_name);

    dd_save_text(dd, FILENAME_REASON, reason);

    g_autofree char *cursor = NULL;
    if (abrt_journal_get_cursor(info->ci_journal, &cursor) == 0)
        dd_save_text(dd, "journald_cursor", cursor);

    const char *data = NULL;
    size_t data_len = 0;

    /* This journal field is not present most of the time, because it is
     * created only for coredumps from processes running in a container.
     *
     * Printing out the log message would be confusing hence.
     *
     * If we find more similar fields, we should not add more if statements
     * but encode this in the struct field_mapping.
     *
     * For now, it would be just vasting of memory and time.
     */
    if (!abrt_journal_get_field(info->ci_journal, "COREDUMP_CONTAINER_CMDLINE", (const void **)&data, &data_len))
    {
        dd_save_binary(dd, FILENAME_CONTAINER_CMDLINE, data, data_len);
    }

    for (size_t i = 0; i < info->ci_mapping_items; ++i)
    {
        const char *data;
        size_t data_len;
        struct field_mapping *f = info->ci_mapping + i;

        if (abrt_journal_get_field(info->ci_journal, f->name, (const void **)&data, &data_len))
        {
            log_info("systemd-coredump journald message misses field: '%s'", f->name);
            continue;
        }

        dd_save_binary(dd, f->file, data, data_len);
    }

    return 0;
}

static int
abrt_journal_core_to_abrt_problem(struct crash_info *info, const char *dump_location)
{
    struct dump_dir *dd = create_dump_dir_ext(dump_location, "ccpp", info->ci_pid, /*fs owner*/0,
            (save_data_call_back)save_systemd_coredump_in_dump_directory, info);

    if (dd != NULL)
    {
        g_autofree char *path = g_strdup(dd->dd_dirname);
        dd_close(dd);
        abrt_notify_new_path(path);
        log_debug("ABRT daemon has been notified about directory: '%s'", path);
    }

    return dd == NULL;
}

/*
 * Prints a core info to stdout.
 */
static int
abrt_journal_core_to_stdout(struct crash_info *info)
{
    printf(_("UID=%9i; SIG=%2i (%4s); EXE=%s\n"),
           info->ci_uid,
           info->ci_signal_no,
           info->ci_signal_name,
           info->ci_executable_path);
    return 0;
}

/*
 * Creates an abrt problem from a journal message
 */
static int
abrt_journal_dump_core(abrt_journal_t *journal, const char *dump_location, int run_flags)
{
    struct crash_info info = { 0 };
    info.ci_journal = journal;
    info.ci_mapping = fields;
    info.ci_mapping_items = sizeof(fields)/sizeof(*fields);

    /* Compatibility hack, a watch's callback gets the journal already moved
     * to a next message. */
    abrt_journal_next(journal);

    /* This the watch call back mentioned in the comment above. We use the
     * following function also in abrt_journal_watch_cores(). */
    int r = abrt_journal_core_retrieve_information(journal, &info);
    if (r != 0)
    {
        if (r < 0)
            error_msg(_("Failed to obtain all required information from journald"));

        goto dump_cleanup;
    }

    if ((run_flags & ABRT_CORE_PRINT_STDOUT))
        r = abrt_journal_core_to_stdout(&info);
    else
        r = abrt_journal_core_to_abrt_problem(&info, dump_location);

dump_cleanup:
    if (info.ci_executable_path != NULL)
        g_free(info.ci_executable_path);

    return r;
}

/*
 * A function called when a new journal core is detected.
 *
 * The function retrieves information from journal, checks the last occurrence
 * time of the crashed executable and if there was no recent occurrence creates
 * an ABRT problem from the journal message. Finally updates the last occurrence
 * time.
 */
static void
abrt_journal_watch_cores(abrt_journal_watch_t *watch, void *user_data)
{
    const abrt_watch_core_conf_t *conf = (const abrt_watch_core_conf_t *)user_data;

    struct crash_info info = { 0 };
    info.ci_journal = abrt_journal_watch_get_journal(watch);
    info.ci_mapping = fields;
    info.ci_mapping_items = sizeof(fields)/sizeof(*fields);

    int r = abrt_journal_core_retrieve_information(abrt_journal_watch_get_journal(watch), &info);
    if (r)
    {
        if (r < 0)
            error_msg(_("Failed to obtain all required information from journald"));

        goto watch_cleanup;
    }

    // do not dump too often
    //   ignore crashes of a single executable appearing in THROTTLE s (keep last 10 executable)
    const time_t current = time(NULL);
    const time_t last = abrt_journal_get_last_occurrence(info.ci_executable_path);

    if (current < last)
    {
        error_msg("BUG: current time stamp lower than an old one");

        if (libreport_g_verbose > 2)
            abort();

        goto watch_cleanup;
    }

    const double sub = difftime(current, last);
    if (sub < conf->awc_throttle)
    {
        /* We don't want to update the counter here. */
        error_msg(_("Not saving repeating crash after %.0fs (limit is %ds)"), sub, conf->awc_throttle);
        goto watch_cleanup;
    }

    if ((conf->awc_run_flags & ABRT_CORE_PRINT_STDOUT))
    {
        if (abrt_journal_core_to_stdout(&info))
        {
            error_msg(_("Failed to print detect problem data to stdout"));
            goto watch_cleanup;
        }
    }
    else
    {
        if (abrt_journal_core_to_abrt_problem(&info, conf->awc_dump_location))
        {
            error_msg(_("Failed to save detect problem data in abrt database"));
            goto watch_cleanup;
        }
    }

    abrt_journal_update_occurrence(info.ci_executable_path, current);

watch_cleanup:
    abrt_journal_save_current_position(info.ci_journal, ABRT_JOURNAL_WATCH_STATE_FILE);

    if (info.ci_executable_path != NULL)
        g_free(info.ci_executable_path);

    return;
}

static void
watch_journald(abrt_journal_t *journal, abrt_watch_core_conf_t *conf)
{
    abrt_journal_watch_t *watch = NULL;
    if (abrt_journal_watch_new(&watch, journal, abrt_journal_watch_cores, (void *)conf) < 0)
        error_msg_and_die(_("Failed to initialize systemd-journal watch"));

    abrt_journal_watch_run_sync(watch);
    abrt_journal_watch_free(watch);
}

int
main(int argc, char *argv[])
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
        "Extract coredumps from systemd-journal\n"
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
        OPT_d = 1 << 2,
        OPT_D = 1 << 3,
        OPT_c = 1 << 4,
        OPT_e = 1 << 5,
        OPT_t = 1 << 6,
        OPT_T = 1 << 7,
        OPT_f = 1 << 8,
        OPT_a = 1 << 9,
        OPT_J = 1 << 10,
        OPT_o = 1 << 11,
    };

    char *cursor = NULL;
    char *dump_location = NULL;
    char *journal_dir = NULL;
    int throttle = 0;
    int run_flags = 0;

    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&libreport_g_verbose),
        OPT_BOOL(  's', NULL, NULL, _("Log to syslog")),
        OPT_STRING('d', NULL, &dump_location, "DIR", _("Create new problem directory in DIR for every coredump")),
        OPT_BOOL(  'D', NULL, NULL, _("Same as -d DumpLocation, DumpLocation is specified in abrt.conf")),
        OPT_STRING('c', NULL, &cursor, "CURSOR", _("Start reading systemd-journal from the CURSOR position")),
        OPT_BOOL(  'e', NULL, NULL, _("Start reading systemd-journal from the end")),
        OPT_INTEGER('t', NULL, &throttle, _("Throttle problem directory creation to 1 per INT second")),
        OPT_BOOL(  'T', NULL, NULL, _("Same as -t INT, INT is specified in plugins/CCpp.conf")),
        OPT_BOOL(  'f', NULL, NULL, _("Follow systemd-journal from the last seen position (if available)")),
        OPT_BOOL(  'a', NULL, NULL, _("Read journal files from all machines")),
        OPT_STRING('J', NULL, &journal_dir,  "PATH", _("Read all journal files from directory at PATH")),
        OPT_BOOL(  'o', NULL, NULL, _("Print found oopses on standard output")),
        OPT_END()
    };
    unsigned opts = libreport_parse_opts(argc, argv, program_options, program_usage_string);

    libreport_export_abrt_envvars(0);

    if ((opts & OPT_s) || getenv("ABRT_SYSLOG"))
        libreport_logmode = LOGMODE_JOURNAL;

    if ((opts & OPT_c) && (opts & OPT_e))
        error_msg_and_die(_("You need to specify either -c CURSOR or -e"));

    /* Initialize ABRT configuration */
    abrt_load_abrt_conf();

    if ((opts & OPT_o))
        run_flags |= ABRT_CORE_PRINT_STDOUT;

    if (opts & OPT_D)
    {
        if (opts & OPT_d)
            libreport_show_usage_and_die(program_usage_string, program_options);
        dump_location = abrt_g_settings_dump_location;
    }

    {   /* Load CCpp.conf */
        g_autoptr(GHashTable) settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        abrt_load_abrt_plugin_conf_file("CCpp.conf", settings);
        const char *value;

        value = g_hash_table_lookup(settings, "VerboseLog");
        if (value)
        {
            char *endptr;

            long verbose = g_ascii_strtoull(value, &endptr, 10);
            if (verbose >= 0 && verbose <= UINT_MAX && value != endptr)
                libreport_g_verbose = (unsigned)verbose;
            else
                error_msg_and_die("expected number in range <%d, %d>: '%s'", 0, UINT_MAX, value);
        }
    }

    /* systemd-coredump creates journal messages with SYSLOG_IDENTIFIER equals
     * 'systemd-coredump' and we are interested only in the systemd-coredump
     * messages.
     *
     * Of cores, it is possible to override this when need while debugging.
     */
    const char *const env_journal_filter = getenv("ABRT_DUMP_JOURNAL_CORE_DEBUG_FILTER");
    GList *coredump_journal_filter = NULL;
    coredump_journal_filter = g_list_append(coredump_journal_filter,
           (env_journal_filter ? (gpointer)env_journal_filter : (gpointer)"SYSLOG_IDENTIFIER=systemd-coredump"));

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

    if (opts & OPT_e) {
        if (abrt_journal_seek_tail(journal) < 0)
            error_msg_and_die(_("Cannot seek to the end of journal"));

        if (abrt_journal_save_current_position(journal, ABRT_JOURNAL_WATCH_STATE_FILE) < 0)
            log_warning("Failed to save the starting cursor position");
    }

    if (cursor && abrt_journal_set_cursor(journal, cursor))
        error_msg_and_die(_("Failed to set systemd-journal cursor '%s'"), cursor);

    if (abrt_journal_set_journal_filter(journal, coredump_journal_filter) < 0)
        error_msg_and_die(_("Cannot filter systemd-journal to systemd-coredump data only"));

    g_list_free(coredump_journal_filter);

    if ((opts & OPT_f))
    {
        if (!cursor && !(opts & OPT_e))
        {
            abrt_journal_restore_position(journal, ABRT_JOURNAL_WATCH_STATE_FILE);

            /* The stored position has already been seen, so move to the next one. */
            abrt_journal_next(journal);
        }

        abrt_watch_core_conf_t conf = {
            .awc_dump_location = dump_location,
            .awc_throttle = throttle,
            .awc_run_flags = run_flags,
        };

        watch_journald(journal, &conf);

        abrt_journal_save_current_position(journal, ABRT_JOURNAL_WATCH_STATE_FILE);
    }
    else
        abrt_journal_dump_core(journal, dump_location, run_flags);

    abrt_journal_free(journal);
    abrt_free_abrt_conf_data();

    return EXIT_SUCCESS;
}
