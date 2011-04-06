/*
    MiddleWare.cpp

    Copyright (C) 2009  Zdenek Prikryl (zprikryl@redhat.com)
    Copyright (C) 2009  RedHat inc.

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
#include "abrtlib.h"
#include "Settings.h"
#include "comm_layer_inner.h"
#include "CommLayerServerDBus.h"
#include "MiddleWare.h"

using namespace std;

/**
 * Get one crash info. If getting is successful,
 * then crash info is filled.
 * @param dump_dir_name A dump dir containing all necessary data.
 * @param pCrashData A crash info.
 * @return It return results of operation. See mw_result_t.
 */
static crash_data_t *FillCrashInfo(const char *dump_dir_name);

/**
 * Transforms a debugdump directory to inner crash
 * report form. This form is used for later reporting.
 * @param dump_dir_name A debugdump dir containing all necessary data.
 * @param pCrashData A created crash report.
 */
static crash_data_t *DebugDumpToCrashReport(const char *dump_dir_name)
{
    VERB3 log(" DebugDumpToCrashReport('%s')", dump_dir_name);

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return NULL;

    static const char *const must_have_files[] = {
	FILENAME_ARCHITECTURE,
	FILENAME_KERNEL      ,
	FILENAME_PACKAGE     ,
	FILENAME_COMPONENT   ,
	FILENAME_OS_RELEASE  ,
	FILENAME_EXECUTABLE  ,
	NULL
    };
    const char *const *v = must_have_files;
    while (*v)
    {
        if (!dd_exist(dd, *v))
        {
            /* Old dump dir format compat. Remove in abrt-2.1 */
            if (strcmp(*v, FILENAME_OS_RELEASE) == 0)
                if (dd_exist(dd, "release"))
                    goto ok;

            dd_close(dd);
            log("Important file '%s/%s' is missing", dump_dir_name, *v);
            return NULL;
        }
 ok:
        v++;
    }

    crash_data_t *crash_data = create_crash_data_from_dump_dir(dd);
    char *events = list_possible_events(dd, NULL, "");
    dd_close(dd);

    add_to_crash_data_ext(crash_data, CD_EVENTS, events,
                          CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);
    free(events);

    add_to_crash_data_ext(crash_data, CD_DUMPDIR, dump_dir_name,
                          CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);

    return crash_data;
}

static char *do_log_and_update_client(char *log_line, void *param)
{
    VERB1 log("%s", log_line);
    update_client("%s", log_line);
    return log_line;
}

/**
 * Takes care of getting all additional data needed
 * for computing UUIDs and creating a report for particular analyzer
 * plugin. This report could be send somewhere afterwards. If a creation
 * is successful, then  a crash report is filled.
 * @param pAnalyzer A name of an analyzer plugin.
 * @param pDebugDumpPath A debugdump dir containing all necessary data.
 * @param pCrashData A filled crash report.
 * @return It return results of operation. See mw_result_t.
 */
/*
 * Called in two cases:
 * (1) by StartJob dbus call -> CreateReportThread(), in the thread
 * (2) by CreateReport dbus call
 */
static mw_result_t CreateCrashReport(const char *dump_dir_name,
                long caller_uid,
                int force,
                crash_data_t **crash_data)
{
    VERB2 log("CreateCrashReport('%s',%ld)", dump_dir_name, caller_uid);

    *crash_data = NULL;

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return MW_NOENT_ERROR;

    struct run_event_state *run_state;
    int res;
    mw_result_t r = MW_OK;

    if (caller_uid != 0) /* not called by root */
    {
        char caller_uid_str[sizeof(long) * 3 + 2];
        sprintf(caller_uid_str, "%ld", caller_uid);

        char *uid = dd_load_text_ext(dd, FILENAME_UID, DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
        bool uid_matches = (uid == NULL || strcmp(uid, caller_uid_str) == 0);
        free(uid);
        if (!uid_matches)
        {
            dd_close(dd);
            error_msg("Dump directory '%s' can't be accessed by user with uid %ld",
                      dump_dir_name, caller_uid);
            r = MW_PERM_ERROR;
            goto ret;
        }
    }
    dd_close(dd);

    run_state = new_run_event_state();
    run_state->logging_callback = do_log_and_update_client;
    res = run_event_on_dir_name(run_state, dump_dir_name, force ? "reanalyze" : "analyze");
    free_run_event_state(run_state);
    if (res != 0)
    {
        r = MW_PLUGIN_ERROR;
        goto ret;
    }

    /* Do a create_crash_data_from_dump_dir from (possibly updated)
     * crash dump dir
     */
    *crash_data = DebugDumpToCrashReport(dump_dir_name);
    if (!*crash_data)
    {
        error_msg("Error loading crash data");
        r = MW_ERROR;
    }

 ret:
    if (*crash_data == NULL)
        *crash_data = new_crash_data();
    VERB3 log("CreateCrashReport() returns %d", r);
    return r;
}

struct logging_state {
    char *last_line;
};

static char *do_log_and_save_line(char *log_line, void *param)
{
    struct logging_state *l_state = (struct logging_state *)param;

    VERB1 log("%s", log_line);
    update_client("%s", log_line);
    free(l_state->last_line);
    l_state->last_line = log_line;
    return NULL;
}

// Do not trust client_report here!
// dbus handler passes it from user without checking
report_status_t Report(crash_data_t *client_report,
                       const vector_string_t& events,
                       const map_map_string_t& settings,
                       long caller_uid)
{
    report_status_t ret;
    const char *dump_dir_name = get_crash_item_content_or_NULL(client_report, CD_DUMPDIR);
    if (!dump_dir_name)
    {
        update_client("Reporting error: %s", "DUMPDIR is missing in client's report data");
        ret[""].push_back("0");      // REPORT_STATUS_IDX_FLAG
        ret[""].push_back("DUMPDIR is missing in client's report data"); // REPORT_STATUS_IDX_MSG
        return ret;
    }

    // Retrieve corresponding stored record
    crash_data_t *stored_report = FillCrashInfo(dump_dir_name);
    if (!stored_report)
    {
        return report_status_t();
    }

    // Is it allowed for this user to report?
    if (caller_uid != 0)   // not called by root
    {
        char caller_uid_str[sizeof(long)*3 + 2];
        sprintf(caller_uid_str, "%ld", caller_uid);
        const char *uid = get_crash_item_content_or_NULL(stored_report, FILENAME_UID);
        if (uid && strcmp(caller_uid_str, uid) != 0)
        {
            free_crash_data(stored_report);
            char *errmsg = xasprintf("user with uid %ld can't report crash %s", caller_uid, dump_dir_name);
            update_client("Reporting error: %s", errmsg);
            ret[""].push_back("0");      // REPORT_STATUS_IDX_FLAG
            ret[""].push_back(errmsg); // REPORT_STATUS_IDX_MSG
            free(errmsg);
            return ret;
        }
    }

    // Save comment, backtrace
//TODO: we should iterate through stored_report and modify all
//modifiable fields which have new data in client_report
    const char *comment = get_crash_item_content_or_NULL(client_report, FILENAME_COMMENT);
    const char *backtrace = get_crash_item_content_or_NULL(client_report, FILENAME_BACKTRACE);
    if (comment || backtrace)
    {
        struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
        if (dd)
        {
            if (comment)
            {
                dd_save_text(dd, FILENAME_COMMENT, comment);
                add_to_crash_data_ext(stored_report, FILENAME_COMMENT, comment, CD_FLAG_TXT + CD_FLAG_ISEDITABLE);
            }
            if (backtrace)
            {
                dd_save_text(dd, FILENAME_BACKTRACE, backtrace);
                add_to_crash_data_ext(stored_report, FILENAME_BACKTRACE, backtrace, CD_FLAG_TXT + CD_FLAG_ISEDITABLE);
            }
            dd_close(dd);
        }
    }

    /* Remove BIN filenames from stored_report if they are not present in client's data */
    GHashTableIter iter;
    char *name;
    struct crash_item *value;
    g_hash_table_iter_init(&iter, stored_report);
    while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
    {
        if (value->flags & CD_FLAG_BIN)
        {
            if (get_crash_item_content_or_NULL(client_report, name) == NULL)
            {
                /* client does not have it -> does not want it passed to events */
                VERB3 log("Won't report BIN file %s:'%s'", name, value->content);
                g_hash_table_iter_remove(&iter);
                continue;
            }
        }
    }

    VERB3 {
        log_crash_data(client_report, " client_report");
        log_crash_data(stored_report, " stored_report");
    }
    free_crash_data(stored_report);
#define stored_report stored_report_must_not_be_used_below
#define client_report client_report_must_not_be_used_below

    // Export overridden settings as environment variables
    GList *env_list = NULL;
    map_map_string_t::const_iterator reporter_settings = settings.begin();
    while (reporter_settings != settings.end())
    {
        map_string_t::const_iterator var = reporter_settings->second.begin();
        while (var != reporter_settings->second.end())
        {
            char *s = xasprintf("%s_%s=%s", reporter_settings->first.c_str(), var->first.c_str(), var->second.c_str());
            VERB3 log("Exporting '%s'", s);
            putenv(s);
            env_list = g_list_append(env_list, s);
            var++;
        }
        reporter_settings++;
    }

    // Run events
    bool at_least_one_reporter_succeeded = false;
    std::string message;
    struct logging_state l_state;
    struct run_event_state *run_state = new_run_event_state();
    run_state->logging_callback = do_log_and_save_line;
    run_state->logging_param = &l_state;
    for (unsigned i = 0; i < events.size(); i++)
    {
        std::string event = events[i];

        l_state.last_line = NULL;
        int r = run_event_on_dir_name(run_state, dump_dir_name, event.c_str());
        if (r == 0 && run_state->children_count == 0)
        {
            l_state.last_line = xasprintf("Error: no processing is specified for event '%s'", event.c_str());
            r = -1;
        }
        if (r == 0)
        {
            at_least_one_reporter_succeeded = true;
            ret[event].push_back("1"); // REPORT_STATUS_IDX_FLAG
            ret[event].push_back(l_state.last_line ? : "Reporting succeeded"); // REPORT_STATUS_IDX_MSG
            if (message != "")
                message += ";";
            message += (l_state.last_line ? : "Reporting succeeded");
        }
        else
        {
            ret[event].push_back("0");      // REPORT_STATUS_IDX_FLAG
            ret[event].push_back(l_state.last_line ? : "Error in reporting"); // REPORT_STATUS_IDX_MSG
            update_client("Reporting via '%s' was not successful%s%s",
                    event.c_str(),
                    l_state.last_line ? ": " : "",
                    l_state.last_line ? l_state.last_line : ""
            );
        }
        free(l_state.last_line);
    }
    free_run_event_state(run_state);

    // Unexport overridden settings
    for (GList *li = env_list; li; li = g_list_next(li))
    {
        char *s = (char*)li->data;
        VERB3 log("Unexporting '%s'", s);
        safe_unsetenv(s);
        free(s);
    }
    g_list_free(env_list);

    // Save reporting results
    if (at_least_one_reporter_succeeded)
    {
        report_status_t::iterator ret_it = ret.begin();
        while (ret_it != ret.end())
        {
//            const string &event = ret_it->first;
//            const vector_string_t &v = ret_it->second;
//            if (v[REPORT_STATUS_IDX_FLAG] == "1")
//            {
// TODO: append to a log of reports done
//                database->SetReportedPerReporter(dump_dir_name, event.c_str(), v[REPORT_STATUS_IDX_MSG].c_str());
//            }
            ret_it++;
        }
        /* Was: database->SetReported(dump_dir_name, message.c_str()); */
        struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
        if (dd)
        {
            add_reported_to(dd, message.c_str());
            dd_close(dd);
        }
    }

    return ret;
#undef stored_report
#undef client_report
}

/* We need to share some data between LoadDebugDump and is_crash_a_dup: */
struct cdump_state {
    char *uid;                   /* filled by LoadDebugDump */
    char *uuid;                  /* filled by is_crash_a_dup */
    char *crash_dump_dup_name;   /* filled by is_crash_a_dup */
};

static int is_crash_a_dup(const char *dump_dir_name, void *param)
{
    struct cdump_state *state = (struct cdump_state *)param;

    if (state->uuid)
        return 0; /* we already checked it, don't do it again */

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 0; /* wtf? (error, but will be handled elsewhere later) */
    state->uuid = dd_load_text_ext(dd, FILENAME_UUID,
                DD_FAIL_QUIETLY_ENOENT + DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE
    );
    dd_close(dd);
    if (!state->uuid)
    {
        return 0; /* no uuid (yet), "run_event, please continue iterating" */
    }

    /* Scan crash dumps looking for a dup */
//TODO: explain why this is safe wrt concurrent runs
    DIR *dir = opendir(DEBUG_DUMPS_DIR);
    if (dir != NULL)
    {
        struct dirent *dent;
        while ((dent = readdir(dir)) != NULL)
        {
            if (dot_or_dotdot(dent->d_name))
                continue; /* skip "." and ".." */

            int different;
            char *uid, *uuid;
            char *dump_dir_name2 = concat_path_file(DEBUG_DUMPS_DIR, dent->d_name);

            if (strcmp(dump_dir_name, dump_dir_name2) == 0)
                goto next; /* we are never a dup of ourself */

            dd = dd_opendir(dump_dir_name2, /*flags:*/ DD_FAIL_QUIETLY_ENOENT);
            if (!dd)
                goto next;
            uid = dd_load_text(dd, FILENAME_UID);
            uuid = dd_load_text(dd, FILENAME_UUID);
            dd_close(dd);
            different = strcmp(state->uid, uid) || strcmp(state->uuid, uuid);
            free(uid);
            free(uuid);
            if (different)
                goto next;

            state->crash_dump_dup_name = dump_dir_name2;
            /* "run_event, please stop iterating": */
            return 1;

 next:
            free(dump_dir_name2);
        }
        closedir(dir);
    }

    /* No dup found */
    return 0; /* "run_event, please continue iterating" */
}

static char *do_log(char *log_line, void *param)
{
    VERB1 log("%s", log_line);
    //update_client("%s", log_line);
    return log_line;
}

mw_result_t LoadDebugDump(const char *dump_dir_name, crash_data_t **crash_data)
{
    mw_result_t res;

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return MW_ERROR;
    struct cdump_state state;
    state.uid = dd_load_text(dd, FILENAME_UID);
    state.uuid = NULL;
    state.crash_dump_dup_name = NULL;
    char *analyzer = dd_load_text(dd, FILENAME_ANALYZER);
    dd_close(dd);

    res = MW_ERROR;

    /* Run post-create event handler(s) */
    struct run_event_state *run_state = new_run_event_state();
    run_state->post_run_callback = is_crash_a_dup;
    run_state->post_run_param = &state;
    run_state->logging_callback = do_log;
    int r = run_event_on_dir_name(run_state, dump_dir_name, "post-create");
    free_run_event_state(run_state);

//TODO: consider this case:
// new dump is created, post-create detects that it is a dup,
// but then FillCrashInfo(dup_name) *FAILS*.
// In this case, we later delete damaged dup_name (right?)
// but new dump never gets its FILENAME_COUNT set!

    /* Is crash a dup? (In this case, is_crash_a_dup() should have
     * aborted "post-create" event processing as soon as it saw uuid
     * and determined that there is another crash with same uuid.
     * In this case it sets state.crash_dump_dup_name)
     */
    if (!state.crash_dump_dup_name)
    {
        /* No. Was there error on one of processing steps in run_event? */
        if (r != 0)
            goto ret; /* yes */

        /* Was uuid created after all? (In this case, is_crash_a_dup()
         * should have fetched it and created state.uuid)
         */
        if (!state.uuid)
        {
            /* no */
            log("Dump directory '%s' has no UUID element", dump_dir_name);
            goto ret;
        }
    }
    else
    {
        dump_dir_name = state.crash_dump_dup_name;
    }

    /* Loads crash_data (from the *first debugdump dir* if this one is a dup)
     * Returns:
     * MW_OCCURRED: "crash count is != 1" (iow: it is > 1 - dup)
     * MW_OK: "crash count is 1" (iow: this is a new crash, not a dup)
     * else: an error code
     */
    {
        dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
        if (!dd)
        {
            res = MW_ERROR;
            goto ret;
        }

        /* Reset mode/uig/gid to correct values for all files created by event run */
        dd_sanitize_mode_and_owner(dd);

        /* Update count */
        char *count_str = dd_load_text_ext(dd, FILENAME_COUNT, DD_FAIL_QUIETLY_ENOENT);
        unsigned long count = strtoul(count_str, NULL, 10);
        count++;
        char new_count_str[sizeof(long)*3 + 2];
        sprintf(new_count_str, "%lu", count);
        dd_save_text(dd, FILENAME_COUNT, new_count_str);
        dd_close(dd);

        *crash_data = FillCrashInfo(dump_dir_name);
        if (*crash_data != NULL)
        {
            res = MW_OK;
            if (count > 1)
            {
                log("Dump directory is a duplicate of %s", dump_dir_name);
                res = MW_OCCURRED;
            }
        }
    }

 ret:
    free(state.uuid);
    free(state.uid);
    free(state.crash_dump_dup_name);
    free(analyzer);

    return res;
}

static crash_data_t *FillCrashInfo(const char *dump_dir_name)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return NULL;

    crash_data_t *crash_data = create_crash_data_from_dump_dir(dd);
    char *events = list_possible_events(dd, NULL, "");
    dd_close(dd);

    add_to_crash_data_ext(crash_data, CD_EVENTS, events,
                          CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);
    free(events);

    add_to_crash_data_ext(crash_data, CD_DUMPDIR, dump_dir_name,
                          CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);

    return crash_data;
}

vector_of_crash_data_t *GetCrashInfos(long caller_uid)
{
    vector_of_crash_data_t *retval = new_vector_of_crash_data();
    log("Getting crash infos...");

    DIR *dir = opendir(DEBUG_DUMPS_DIR);
    if (dir != NULL)
    {
        struct dirent *dent;
        while ((dent = readdir(dir)) != NULL)
        {
            if (dot_or_dotdot(dent->d_name))
                continue; /* skip "." and ".." */

            char *dump_dir_name = concat_path_file(DEBUG_DUMPS_DIR, dent->d_name);

            struct stat statbuf;
            if (stat(dump_dir_name, &statbuf) != 0
             || !S_ISDIR(statbuf.st_mode)
            ) {
                goto next; /* not a dir, skip */
            }

            /* Skip directories which are not for this uid */
            if (caller_uid != 0) /* not called by root? */
            {
                char *uid;
                char caller_uid_str[sizeof(long) * 3 + 2];

                struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
                if (!dd)
                    goto next;

                sprintf(caller_uid_str, "%ld", caller_uid);
                uid = dd_load_text(dd, FILENAME_UID);
                bool uid_matches = (uid == NULL || strcmp(uid, caller_uid_str) == 0);
                free(uid);
                dd_close(dd);
                if (!uid_matches)
                {
                    goto next;
                }
            }

            {
                crash_data_t *crash_data = FillCrashInfo(dump_dir_name);
                if (!crash_data)
                {
                    error_msg("Dump directory %s doesn't exist or misses crucial files, deleting", dump_dir_name);
                    delete_dump_dir(dump_dir_name);
                }
                else
                {
                    g_ptr_array_add(retval, crash_data);
                }
            }
 next:
            free(dump_dir_name);
        }
        closedir(dir);
    }

    return retval;
}

/*
 * Called in two cases:
 * (1) by StartJob dbus call -> CreateReportThread(), in the thread
 * (2) by CreateReport dbus call
 * In the second case, it finishes quickly, because previous
 * StartJob dbus call already did all the processing, and we just retrieve
 * the result from dump directory, which is fast.
 */
void CreateReport(const char* crash_id, long caller_uid, int force, crash_data_t **crash_data)
{
    /* FIXME: starting from here, any shared data must be protected with a mutex. */
    mw_result_t res = CreateCrashReport(crash_id, caller_uid, force, crash_data);
    switch (res)
    {
        case MW_OK:
            VERB2 log_crash_data(*crash_data, "crashReport");
            break;
        case MW_NOENT_ERROR:
            error_msg("Can't find crash with id '%s'", crash_id);
            break;
        case MW_PERM_ERROR:
            error_msg("Can't find crash with id '%s'", crash_id);
            break;
        case MW_PLUGIN_ERROR:
            error_msg("Particular analyzer plugin isn't loaded or there is an error within plugin(s)");
            break;
        default:
            error_msg("Corrupted crash with id %s, deleting", crash_id);
            DeleteDebugDump(crash_id, /*caller_uid:*/ 0);
            break;
    }
}

typedef struct thread_data_t {
    pthread_t thread_id;
    long caller_uid;
    int force;
    char* crash_id;
    char* peer;
} thread_data_t;
static void* create_report(void* arg)
{
    thread_data_t *thread_data = (thread_data_t *) arg;

    /* Client name is per-thread, need to set it */
    set_client_name(thread_data->peer);

    log("Creating report...");
    crash_data_t *crash_data = NULL;
    CreateReport(thread_data->crash_id, thread_data->caller_uid, thread_data->force, &crash_data);
    send_dbus_sig_JobDone(thread_data->peer);
    set_client_name(NULL);

    /* free strduped strings */
    free(thread_data->crash_id);
    free(thread_data->peer);
    free(thread_data);

    /* Bogus value. pthreads require us to return void* */
    return NULL;
}
int CreateReportThread(const char* crash_id, long caller_uid, int force, const char* pSender)
{
    thread_data_t *thread_data = (thread_data_t *)xzalloc(sizeof(thread_data_t));
    thread_data->crash_id = xstrdup(crash_id);
    thread_data->caller_uid = caller_uid;
    thread_data->force = force;
    thread_data->peer = xstrdup(pSender);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int r = pthread_create(&thread_data->thread_id, &attr, create_report, thread_data);
    pthread_attr_destroy(&attr);
    if (r != 0)
    {
        free(thread_data->crash_id);
        free(thread_data->peer);
        free(thread_data);
        /* The only reason this may happen is system-wide resource starvation,
         * or ulimit is exceeded (someone floods us with CreateReport() dbus calls?)
         */
        error_msg("Can't create thread");
        return r;
    }
    VERB3 log("Thread %llx created", (unsigned long long)thread_data->thread_id);
    return r;
}


/* Remove dump dir */
int DeleteDebugDump(const char *dump_dir_name, long caller_uid)
{
    /* If doesn't start with "DEBUG_DUMPS_DIR/"... */
    if (strncmp(dump_dir_name, DEBUG_DUMPS_DIR"/", strlen(DEBUG_DUMPS_DIR"/")) != 0
    /* or contains "/." anywhere (-> might contain ".." component) */
     || strstr(dump_dir_name + strlen(DEBUG_DUMPS_DIR), "/.")
    ) {
        /* Then refuse to operate on it (someone is attacking us??) */
        error_msg("Bad dump directory name '%s', not deleting", dump_dir_name);
        return MW_ERROR;
    }

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return MW_NOENT_ERROR;

    if (caller_uid != 0) /* not called by root */
    {
        char caller_uid_str[sizeof(long) * 3 + 2];
        sprintf(caller_uid_str, "%ld", caller_uid);

        char *uid = dd_load_text_ext(dd, FILENAME_UID, DD_FAIL_QUIETLY_ENOENT | DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE);
        /* we assume that the dump_dir can be handled by everyone if uid == NULL
         * e.g: kerneloops
         */
        if (uid != NULL)
        {
            bool uid_matches = (strcmp(uid, caller_uid_str) == 0);
            free(uid);
            if (!uid_matches)
            {
                dd_close(dd);
                error_msg("Dump directory '%s' can't be accessed by user with uid %ld", dump_dir_name, caller_uid);
                return 1;
            }
        }
    }

    dd_delete(dd);

    return 0; /* success */
}
