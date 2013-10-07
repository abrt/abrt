/*
    Copyright (C) 2011  ABRT Team
    Copyright (C) 2011  RedHat inc.

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
#include <satyr/thread.h>
#include <satyr/stacktrace.h>
#include <satyr/distance.h>
#include <satyr/abrt.h>

#include "libabrt.h"
#include <libreport/run_event.h>

/* 70 % similarity */
#define BACKTRACE_DUP_THRESHOLD 0.3

static char *uid = NULL;
static char *uuid = NULL;
static struct sr_stacktrace *corebt = NULL;
static char *analyzer = NULL;
static char *crash_dump_dup_name = NULL;

static void dup_corebt_fini(void);

static char* load_backtrace(const struct dump_dir *dd)
{
    const char *filename = FILENAME_BACKTRACE;
    if (strcmp(analyzer, "CCpp") == 0)
    {
        filename = FILENAME_CORE_BACKTRACE;
    }

    return dd_load_text_ext(dd, filename,
        DD_FAIL_QUIETLY_ENOENT|DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
}

static int core_backtrace_is_duplicate(struct sr_stacktrace *bt1,
                                       const char *bt2_text)
{
    struct sr_thread *thread1 = sr_stacktrace_find_crash_thread(bt1);

    if (thread1 == NULL)
    {
        VERB1 log("New stacktrace has no crash thread, disabling core stacktrace deduplicate");
        dup_corebt_fini();
        return 0;
    }

    int result;
    char *error_message;
    struct sr_stacktrace *bt2 = sr_stacktrace_parse(sr_abrt_type_from_analyzer(analyzer),
                                                    bt2_text, &error_message);
    if (bt2 == NULL)
    {
        VERB1 log("Failed to parse backtrace, considering it not duplicate: %s", error_message);
        free(error_message);
        return 0;
    }

    struct sr_thread *thread2 = sr_stacktrace_find_crash_thread(bt2);

    if (thread2 == NULL)
    {
        VERB1 log("Failed to get crash thread, considering it not duplicate");
        result = 0;
        goto end;
    }

    int length2 = sr_thread_frame_count(thread2);

    if (length2 <= 0)
    {
        VERB1 log("Core backtrace has zero frames, considering it not duplicate");
        result = 0;
        goto end;
    }

    /* This is an ugly workaround for https://github.com/abrt/btparser/issues/6 */
    /*
    int length1 = sr_core_thread_get_frame_count(thread1);

    if (length1 <= 2 || length2 <= 2)
    {
        VERB1 log("Backtraces too short, falling back on full comparison");
        result = (sr_core_thread_cmp(thread1, thread2) == 0);
        goto end;
    }
    */

    float distance = sr_distance(SR_DISTANCE_DAMERAU_LEVENSHTEIN, thread1, thread2);
    VERB2 log("Distance between backtraces: %f", distance);
    result = (distance <= BACKTRACE_DUP_THRESHOLD);

end:
    sr_stacktrace_free(bt2);

    return result;
}

static void dup_uuid_init(const struct dump_dir *dd)
{
    if (uuid)
        return; /* we already loaded it, don't do it again */

    uuid = dd_load_text_ext(dd, FILENAME_UUID,
                            DD_FAIL_QUIETLY_ENOENT + DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE
    );
}

static int dup_uuid_compare(const struct dump_dir *dd)
{
    char *dd_uuid;
    int different;

    if (!uuid)
        return 0;

    /* don't do uuid-based check on crashes that have backtrace available (and
     * nonempty)
     * XXX: this relies on the fact that backtrace is created in the same event
     * as UUID
     */
    if (corebt)
        return 0;

    dd_uuid = dd_load_text_ext(dd, FILENAME_UUID, DD_FAIL_QUIETLY_ENOENT);
    different = strcmp(uuid, dd_uuid);
    free(dd_uuid);

    if (!different)
        log("Duplicate: UUID");

    return !different;
}

static void dup_uuid_fini(void)
{
    free(uuid);
    uuid = NULL;
}

static void dup_corebt_init(const struct dump_dir *dd)
{
    if (corebt)
        return; /* already loaded */

    char *corebt_text = load_backtrace(dd);
    if (!corebt_text)
        return; /* no backtrace */

    enum sr_report_type report_type = sr_abrt_type_from_analyzer(analyzer);
    if (report_type == SR_REPORT_INVALID)
    {
        VERB1 log("Can't load stacktrace because of unsupported analyzer: %s",
                  analyzer);
        return;
    }

    /* sr_stacktrace_parse moves the pointer */
    char *error_message;
    corebt = sr_stacktrace_parse(report_type, corebt_text, &error_message);
    if (!corebt)
    {
        VERB1 log("Failed to load core stacktrace: %s", error_message);
        free(error_message);
    }

    free(corebt_text);
}

static int dup_corebt_compare(const struct dump_dir *dd)
{
    if (!corebt)
        return 0;

    int isdup;

    char *dd_corebt = load_backtrace(dd);
    if (!dd_corebt)
        return 0;

    isdup = core_backtrace_is_duplicate(corebt, dd_corebt);
    free(dd_corebt);

    if (isdup)
        log("Duplicate: core backtrace");

    return isdup;
}

static void dup_corebt_fini(void)
{
    sr_stacktrace_free(corebt);
    corebt = NULL;
}

/* This function is run after each post-create event is finished (there may be
 * multiple such events).
 *
 * It first checks if there is CORE_BACKTRACE or UUID item in the dump dir
 * we are processing.
 *
 * If there is a CORE_BACKTRACE, it iterates over all other dump
 * directories and computes similarity to their core backtraces (if any).
 * If one of them is similar enough to be considered duplicate, the function
 * saves the path to the dump directory in question and returns 1 to indicate
 * that we have indeed found a duplicate of currently processed dump directory.
 * No more events are processed and program prints the path to the other
 * directory and returns failure.
 *
 * If there is an UUID item (and no core backtrace), the function again
 * iterates over all other dump directories and compares this UUID to their
 * UUID. If there is a match, the path to the duplicate is saved and 1 is returned.
 *
 * If duplicate is not found as described above, the function returns 0 and we
 * either process remaining events if there are any, or successfully terminate
 * processing of the current dump directory.
 */
static int is_crash_a_dup(const char *dump_dir_name, void *param)
{
    int retval = 0; /* defaults to no dup found, "run_event, please continue iterating" */

    struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
    if (!dd)
        return 0; /* wtf? (error, but will be handled elsewhere later) */
    free(analyzer);
    analyzer = dd_load_text(dd, FILENAME_ANALYZER);
    dup_uuid_init(dd);
    dup_corebt_init(dd);
    dd_close(dd);

    /* dump_dir_name can be relative */
    dump_dir_name = realpath(dump_dir_name, NULL);

    DIR *dir = opendir(g_settings_dump_location);
    if (dir == NULL)
        goto end;

    /* Scan crash dumps looking for a dup */
    //TODO: explain why this is safe wrt concurrent runs
    struct dirent *dent;
    while ((dent = readdir(dir)) != NULL && crash_dump_dup_name == NULL)
    {
        if (dot_or_dotdot(dent->d_name))
            continue; /* skip "." and ".." */
        const char *ext = strrchr(dent->d_name, '.');
        if (ext && strcmp(ext, ".new") == 0)
            continue; /* skip anything named "<dirname>.new" */

        dd = NULL;

        char *tmp_concat_path = concat_path_file(g_settings_dump_location, dent->d_name);

        char *dump_dir_name2 = realpath(tmp_concat_path, NULL);
        if (g_verbose > 1 && !dump_dir_name2)
            perror_msg("realpath(%s)", tmp_concat_path);

        free(tmp_concat_path);

        if (!dump_dir_name2)
            continue;

        char *dd_uid = NULL, *dd_analyzer = NULL;

        if (strcmp(dump_dir_name, dump_dir_name2) == 0)
            goto next; /* we are never a dup of ourself */

        dd = dd_opendir(dump_dir_name2, /*flags:*/ DD_FAIL_QUIETLY_ENOENT | DD_OPEN_READONLY);
        if (!dd)
            goto next;

        /* crashes of different users are not considered duplicates */
        dd_uid = dd_load_text_ext(dd, FILENAME_UID, DD_FAIL_QUIETLY_ENOENT);
        if (strcmp(uid, dd_uid))
        {
            goto next;
        }

        /* different crash types are not duplicates */
        dd_analyzer = dd_load_text_ext(dd, FILENAME_ANALYZER, DD_FAIL_QUIETLY_ENOENT);
        if (strcmp(analyzer, dd_analyzer))
        {
            goto next;
        }

        if (dup_uuid_compare(dd)
         || dup_corebt_compare(dd)
        ) {
            crash_dump_dup_name = dump_dir_name2;
            dump_dir_name2 = NULL;
            retval = 1; /* "run_event, please stop iterating" */
            /* sonce crash_dump_dup_name != NULL now, we exit the loop */
        }

next:
        free(dump_dir_name2);
        dd_close(dd);
        free(dd_uid);
        free(dd_analyzer);
    }
    closedir(dir);

end:
    free((char*)dump_dir_name);
    return retval;
}

static void create_lockfile(void)
{
    char pid_str[sizeof(long)*3 + 4];
    sprintf(pid_str, "%lu", (long)getpid());
    char *lock_filename = concat_path_file(g_settings_dump_location, "post-create.lock");

    /* Someone else's post-create may take a long-ish time to finish.
     * For example, I had a failing email sending there, it took
     * a minute to time out.
     * That's why timeout is large (100 seconds):
     */
    int count = 100;
    while (1)
    {
        /* Return values:
         * -1: error (in this case, errno is 0 if error message is already logged)
         *  0: failed to lock (someone else has it locked)
         *  1: success
         */
        int r = create_symlink_lockfile(lock_filename, pid_str);
    if (r > 0)
            break;
    if (r < 0)
            error_msg_and_die("Can't create '%s'", lock_filename);
    if (--count == 0)
        {
            /* Someone else's post-create process is alive but stuck.
             * Don't wait forever.
             */
            error_msg("Stale lock '%s', removing it", lock_filename);
            xunlink(lock_filename);
            break;
        }
        sleep(1);
    }
    free(lock_filename);
}

static void delete_lockfile(void)
{
    char *lock_filename = concat_path_file(g_settings_dump_location, "post-create.lock");
    xunlink(lock_filename);
    free(lock_filename);
}

static char *do_log(char *log_line, void *param)
{
    /* We pipe output of events to our log.
     * Otherwise, errors on post-create result in
     * "Corrupted or bad dump DIR, deleting" without adequate explanation why.
     */
    log("%s", log_line);
    return log_line;
}

int main(int argc, char **argv)
{
    /* I18n */
    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    abrt_init(argv);

    const char *program_usage_string = _(
        "& [-v -i] -e|--event EVENT DIR..."
        );

    char *event_name = NULL;
    int interactive = 0; /* must be _int_, OPT_BOOL expects that! */

    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('e', "event" , &event_name, "EVENT",  _("Run EVENT on DIR")),
        OPT_BOOL('i', "interactive" , &interactive, _("Communicate directly to the user")),
        OPT_END()
    };

    parse_opts(argc, argv, program_options, program_usage_string);
    argv += optind;
    if (!*argv || !event_name)
        show_usage_and_die(program_usage_string, program_options);

    load_abrt_conf();

    bool post_create = (strcmp(event_name, "post-create") == 0);
    char *dump_dir_name = NULL;
    while (*argv)
    {
        dump_dir_name = xstrdup(*argv++);
        int i = strlen(dump_dir_name);
        while (--i >= 0)
            if (dump_dir_name[i] != '/')
                break;
        dump_dir_name[++i] = '\0';

        struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ DD_OPEN_READONLY);
        if (!dd)
            return 1;

        uid = dd_load_text_ext(dd, FILENAME_UID, DD_FAIL_QUIETLY_ENOENT);
        dd_close(dd);

        struct run_event_state *run_state = new_run_event_state();
        if (!interactive)
            make_run_event_state_forwarding(run_state);
        run_state->logging_callback = do_log;
        if (post_create)
        {
            run_state->post_run_callback = is_crash_a_dup;
            /*
             * The post-create event cannot be run concurrently for more problem
             * directories. The problem is in searching for duplicates process
             * in case when two concurrently processed directories are duplicates
             * of each other. Both of the directories are marked as duplicates
             * of each other and are deleted.
             */
            create_lockfile();
        }

        int r = run_event_on_dir_name(run_state, dump_dir_name, event_name);

        if (post_create)
            delete_lockfile();

        free_run_event_state(run_state);
        /* Needed only if is_crash_a_dup() was called, but harmless
         * even if it wasn't:
         */
        dup_uuid_fini();
        dup_corebt_fini();

        if (r == 0 && run_state->children_count == 0)
            error_msg_and_die("No actions are found for event '%s'", event_name);

//TODO: consider this case:
// new dump is created, post-create detects that it is a dup,
// but then load_crash_info(dup_name) *FAILS*.
// In this case, we later delete damaged dup_name (right?)
// but new dump never gets its FILENAME_COUNT set!

        /* Is crash a dup? (In this case, is_crash_a_dup() should have
         * aborted "post-create" event processing as soon as it saw uuid
         * and determined that there is another crash with same uuid.
         * In this case it sets crash_dump_dup_name)
         */
        if (crash_dump_dup_name)
            error_msg_and_die("DUP_OF_DIR: %s", crash_dump_dup_name);

        /* Was there error on one of processing steps in run_event? */
        if (r != 0)
            return r; /* yes */

        free(dump_dir_name);
        dump_dir_name = NULL;
    }

    /* exit 0 means, that there is no duplicate of dump-dir */
    return 0;
}
