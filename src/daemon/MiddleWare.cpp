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
#include "Daemon.h"
#include "Settings.h"
#include "abrt_exception.h"
#include "abrt_packages.h"
#include "comm_layer_inner.h"
#include "MiddleWare.h"

using namespace std;

/**
 * An instance of CPluginManager. When MiddleWare wants to do something
 * with plugins, it calls the plugin manager.
 * @see PluginManager.h
 */
CPluginManager* g_pPluginManager;

/**
 * A map, which associates particular analyzer to one or more
 * action or reporter plugins. These are activated when a crash, which
 * is maintained by particular analyzer, occurs.
 */
typedef std::map<std::string, vector_pair_string_string_t> map_analyzer_actions_and_reporters_t;
static map_analyzer_actions_and_reporters_t s_mapAnalyzerActionsAndReporters;
/**
 * A vector of one or more action or reporter plugins. These are
 * activated when any crash occurs.
 */
static vector_pair_string_string_t s_vectorActionsAndReporters;


/**
 * Transforms a debugdump directory to inner crash
 * report form. This form is used for later reporting.
 * @param pDebugDumpDir A debugdump dir containing all necessary data.
 * @param pCrashData A created crash report.
 */
static bool DebugDumpToCrashReport(const char *pDebugDumpDir, map_crash_data_t& pCrashData)
{
    VERB3 log(" DebugDumpToCrashReport('%s')", pDebugDumpDir);

    struct dump_dir *dd = dd_opendir(pDebugDumpDir, /*flags:*/ 0);
    if (!dd)
        return false;

    const char *const *v = must_have_files;
    while (*v)
    {
        if (!dd_exist(dd, *v))
        {
            dd_close(dd);
            throw CABRTException(EXCEP_ERROR, "DebugDumpToCrashReport(): important file '%s' is missing", *v);
        }

        v++;
    }

    load_crash_data_from_debug_dump(dd, pCrashData);
    dd_close(dd);

    return true;
}

static char *do_log_and_update_client(char *log_line, void *param)
{
    VERB1 log("%s", log_line);
    update_client("%s", log_line);
    return log_line;
}

/*
 * Called in two cases:
 * (1) by StartJob dbus call -> CreateReportThread(), in the thread
 * (2) by CreateReport dbus call
 */
mw_result_t CreateCrashReport(const char *crash_id,
                long caller_uid,
                int force,
                map_crash_data_t& pCrashData)
{
    VERB2 log("CreateCrashReport('%s',%ld,result)", crash_id, caller_uid);

    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
    database->Connect();
    struct db_row *row = database->GetRow(crash_id);
    database->DisConnect();
    if (!row)
    {
        error_msg("crash '%s' is not in database", crash_id);
        return MW_IN_DB_ERROR;
    }

    mw_result_t r = MW_OK;

    char caller_uid_str[sizeof(long) * 3 + 2];
    sprintf(caller_uid_str, "%li", caller_uid);
    if (caller_uid != 0 /* not called by root */
     && row->db_inform_all[0] != '1'
     && strcmp(caller_uid_str, row->db_uid) != 0
    ) {
        error_msg("crash '%s' can't be accessed by user with uid %ld", crash_id, caller_uid);
        r = MW_IN_DB_ERROR;
        goto ret;
    }

    try
    {
        struct run_event_state *run_state = new_run_event_state();
        run_state->logging_callback = do_log_and_update_client;
        int res = run_event(run_state, row->db_dump_dir, force ? "reanalyze" : "analyze");
        free_run_event_state(run_state);
        if (res != 0)
        {
            r = MW_PLUGIN_ERROR;
            goto ret;
        }

        /* Do a load_crash_data_from_debug_dump from (possibly updated)
         * crash dump dir
         */
        if (!DebugDumpToCrashReport(row->db_dump_dir, pCrashData))
        {
            error_msg("Error loading crash data");
            r = MW_ERROR;
            goto ret;
        }
    }
    catch (CABRTException& e)
    {
        r = MW_CORRUPTED;
        error_msg("%s", e.what());
        if (e.type() == EXCEP_PLUGIN)
        {
            r = MW_PLUGIN_ERROR;
        }
    }

 ret:
    db_row_free(row);
    VERB3 log("CreateCrashReport() returns %d", r);
    return r;
}

void RunAction(const char *pActionDir,
                            const char *pPluginName,
                            const char *pPluginArgs)
{
    CAction* action = g_pPluginManager->GetAction(pPluginName);
    if (!action)
    {
        /* GetAction() already complained */
        return;
    }
    try
    {
        action->Run(pActionDir, pPluginArgs, /*force:*/ 0);
    }
    catch (CABRTException& e)
    {
        error_msg("Execution of '%s' was not successful: %s", pPluginName, e.what());
    }
}

void RunActionsAndReporters(const char *pDebugDumpDir)
{
    vector_pair_string_string_t::iterator it_ar = s_vectorActionsAndReporters.begin();
    map_plugin_settings_t plugin_settings;
    for (; it_ar != s_vectorActionsAndReporters.end(); it_ar++)
    {
        const char *plugin_name = it_ar->first.c_str();
        try
        {
            VERB3 log("RunActionsAndReporters: checking %s", plugin_name);
            plugin_type_t tp = g_pPluginManager->GetPluginType(plugin_name);
            if (tp == REPORTER)
            {
                CReporter* reporter = g_pPluginManager->GetReporter(plugin_name); /* can't be NULL */
                map_crash_data_t crashReport;
                if (DebugDumpToCrashReport(pDebugDumpDir, crashReport))
                {
                    VERB2 log("%s.Report(...)", plugin_name);
                    reporter->Report(crashReport, plugin_settings, it_ar->second.c_str());
                }
                else
                    error_msg("Activation of plugin '%s' was not successful: Error converting crash data", plugin_name);
            }
            else if (tp == ACTION)
            {
                CAction* action = g_pPluginManager->GetAction(plugin_name); /* can't be NULL */
                VERB2 log("%s.Run('%s','%s')", plugin_name, pDebugDumpDir, it_ar->second.c_str());
                action->Run(pDebugDumpDir, it_ar->second.c_str(), /*force:*/ 0);
            }
        }
        catch (CABRTException& e)
        {
            error_msg("Activation of plugin '%s' was not successful: %s", plugin_name, e.what());
        }
    }
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
report_status_t Report(const map_crash_data_t& client_report,
                       const vector_string_t &reporters,
                       const map_map_string_t& settings,
                       long caller_uid)
{
    // Get ID fields
    const char *UID = get_crash_data_item_content_or_NULL(client_report, CD_UID);
    const char *UUID = get_crash_data_item_content_or_NULL(client_report, CD_UUID);
    if (!UID || !UUID)
    {
        throw CABRTException(EXCEP_ERROR, "Report(): UID or UUID is missing in client's report data");
    }
    string crash_id = ssprintf("%s:%s", UID, UUID);

    // Retrieve corresponding stored record
    map_crash_data_t stored_report;
    mw_result_t r = FillCrashInfo(crash_id.c_str(), stored_report);
    if (r != MW_OK)
    {
        return report_status_t();
    }

    // Is it allowed for this user to report?
    if (caller_uid != 0   // not called by root
     && get_crash_data_item_content(stored_report, CD_INFORMALL) != "1"
     && strcmp(to_string(caller_uid).c_str(), UID) != 0
    ) {
        throw CABRTException(EXCEP_ERROR, "Report(): user with uid %ld can't report crash %s",
                        caller_uid, crash_id.c_str());
    }

    const char *dump_dir_name = get_crash_data_item_content_or_NULL(stored_report, CD_DUMPDIR);

    // Save comment, "how to reproduce", backtrace
//TODO: we should iterate through stored_report and modify all
//modifiable fields which have new data in client_report
    const char *comment = get_crash_data_item_content_or_NULL(client_report, FILENAME_COMMENT);
    const char *reproduce = get_crash_data_item_content_or_NULL(client_report, FILENAME_REPRODUCE);
    const char *backtrace = get_crash_data_item_content_or_NULL(client_report, FILENAME_BACKTRACE);
    if (comment || reproduce || backtrace)
    {
        struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
        if (dd)
        {
            if (comment)
            {
                dd_save_text(dd, FILENAME_COMMENT, comment);
                add_to_crash_data_ext(stored_report, FILENAME_COMMENT, CD_TXT, CD_ISEDITABLE, comment);
            }
            if (reproduce)
            {
                dd_save_text(dd, FILENAME_REPRODUCE, reproduce);
                add_to_crash_data_ext(stored_report, FILENAME_REPRODUCE, CD_TXT, CD_ISEDITABLE, reproduce);
            }
            if (backtrace)
            {
                dd_save_text(dd, FILENAME_BACKTRACE, backtrace);
                add_to_crash_data_ext(stored_report, FILENAME_BACKTRACE, CD_TXT, CD_ISEDITABLE, backtrace);
            }
            dd_close(dd);
        }
    }

    /* Remove BIN filenames from stored_report if they are not present in client's data */
    map_crash_data_t::const_iterator its = stored_report.begin();
    while (its != stored_report.end())
    {
        if (its->second[CD_TYPE] == CD_BIN)
        {
            std::string key = its->first;
            if (get_crash_data_item_content_or_NULL(client_report, key.c_str()) == NULL)
            {
                /* client does not have it -> does not want it passed to reporters */
                VERB3 log("Won't report BIN file %s:'%s'", key.c_str(), its->second[CD_CONTENT].c_str());
                its++; /* move off the element we will erase */
                stored_report.erase(key);
                continue;
            }
        }
        its++;
    }

    VERB3 {
        log("Run reporters");
        log_map_crash_data(client_report, " client_report");
        log_map_crash_data(stored_report, " stored_report");
    }
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

    // Run reporters
    bool at_least_one_reporter_succeeded = false;
    report_status_t ret;
    std::string message;
    struct logging_state l_state;
    struct run_event_state *run_state = new_run_event_state();
    run_state->logging_callback = do_log_and_save_line;
    run_state->logging_param = &l_state;
    for (unsigned i = 0; i < reporters.size(); i++)
    {
        std::string plugin_name = "report_" + reporters[i];

        l_state.last_line = NULL;
        int r = run_event(run_state, dump_dir_name, plugin_name.c_str());
        if (r == 0)
        {
            at_least_one_reporter_succeeded = true;
            ret[plugin_name].push_back("1"); // REPORT_STATUS_IDX_FLAG
            ret[plugin_name].push_back(l_state.last_line ? : "Reporting succeeded"); // REPORT_STATUS_IDX_MSG
            if (message != "")
                message += ";";
            message += (l_state.last_line ? : "Reporting succeeded");
        }
        else
        {
            ret[plugin_name].push_back("0");      // REPORT_STATUS_IDX_FLAG
            ret[plugin_name].push_back(l_state.last_line ? : "Error in reporting"); // REPORT_STATUS_IDX_MSG
            update_client("Reporting via '%s' was not successful%s%s",
                    plugin_name.c_str(),
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
        /* Need to make a copy: just cutting s at '=' and unsetenv'ing
	 * the result would be a bug! s _itself_ is in environment now,
	 * we must not modify it there!
	 */
        char *name = xstrndup(s, strchrnul(s, '=') - s);
        VERB3 log("Unexporting '%s'", name);
        unsetenv(name);
        free(name);
        free(s);
    }
    g_list_free(env_list);

    // Save reporting results to database
    if (at_least_one_reporter_succeeded)
    {
        CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
        database->Connect();
        report_status_t::iterator ret_it = ret.begin();
        while (ret_it != ret.end())
        {
            const string &plugin_name = ret_it->first;
            const vector_string_t &v = ret_it->second;
            if (v[REPORT_STATUS_IDX_FLAG] == "1")
            {
                database->SetReportedPerReporter(crash_id.c_str(), plugin_name.c_str(), v[REPORT_STATUS_IDX_MSG].c_str());
            }
            ret_it++;
        }
        database->SetReported(crash_id.c_str(), message.c_str());
        database->DisConnect();
    }

    return ret;
#undef client_report
}

/**
 * Check whether particular debugdump directory is saved in database.
 * @param debug_dump_dir
 *  A debugdump dir containing all necessary data.
 * @return
 *  It returns true if debugdump dir is already saved, otherwise
 *  it returns false.
 */
static bool is_debug_dump_saved(const char *debug_dump_dir)
{
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
    database->Connect();
    struct db_row *row = database->GetRow_by_dir(debug_dump_dir);
    database->DisConnect();

    db_row_free(row);
    return row != NULL;
}

bool analyzer_has_InformAllUsers(const char *analyzer_name)
{
    CAnalyzer* analyzer = g_pPluginManager->GetAnalyzer(analyzer_name);
    if (!analyzer)
    {
        return false;
    }
    map_plugin_settings_t settings = analyzer->GetSettings();
    map_plugin_settings_t::const_iterator it = settings.find("InformAllUsers");
    if (it == settings.end())
        return false;
    return string_to_bool(it->second.c_str());
}

/**
 * Save a debugdump into database. If saving is
 * successful, then crash info is filled. Otherwise the crash info is
 * not changed.
 * @param pUUID A local UUID of a crash.
 * @param pUID An UID of an user.
 * @param pTime Time when a crash occurs.
 * @param pDebugDumpPath A debugdump path.
 * @param pCrashData A filled crash info.
 * @return It return results of operation. See mw_result_t.
 */
static mw_result_t SaveDebugDumpToDatabase(const char *crash_id,
                bool inform_all_users,
                const char *pTime,
                const char *pDebugDumpDir,
                map_crash_data_t& pCrashData)
{
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
    database->Connect();
    /* note: if [UUID,UID] record exists, pDebugDumpDir is not updated in the record */
    database->Insert_or_Update(crash_id, inform_all_users, pDebugDumpDir, pTime);

    struct db_row *row = database->GetRow(crash_id);
    database->DisConnect();

    mw_result_t res = FillCrashInfo(crash_id, pCrashData);
    if (res == MW_OK)
    {
        const char *first = get_crash_data_item_content(pCrashData, CD_DUMPDIR).c_str();
        if (row && row->db_reported[0] == '1')
        {
            log("Crash is in database already (dup of %s) and is reported", first);
            db_row_free(row);
            return MW_REPORTED;
        }
        if (row && xatou(row->db_count) > 1)
        {
            db_row_free(row);
            log("Crash is in database already (dup of %s)", first);
            return MW_OCCURRED;
        }
    }
    db_row_free(row);
    return res;
}

/* We need to share some data between SaveDebugDump and is_crash_id_in_db: */
struct cdump_state {
    char *uid;             /* filled by SaveDebugDump */
    char *crash_id;        /* filled by is_crash_id_in_db */
    int crash_id_is_in_db; /* filled by is_crash_id_in_db */
};

static int is_crash_id_in_db(const char *dump_dir_name, void *param)
{
    struct cdump_state *state = (struct cdump_state *)param;

    if (state->crash_id)
        return 0; /* we already checked it, don't do it again */

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return 0; /* wtf? (error, but will be handled elsewhere later) */
    char *uuid = dd_load_text(dd, CD_UUID);
    dd_close(dd);
//TODO: want flag to dd_load_text: "please return NULL if not found"
    if (!uuid[0])
    {
        free(uuid);
        return 0; /* no uuid (yet), "run_event, please continue iterating" */
    }
    state->crash_id = xasprintf("%s:%s", state->uid, uuid);
    free(uuid);

    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
    database->Connect();
    struct db_row *row = database->GetRow(state->crash_id);
    database->DisConnect();

    if (!row) /* Crash id is not in db - this crash wasn't seen before */
        return 0; /* "run_event, please continue iterating" */

    /* Crash id is in db */
    db_row_free(row);
    state->crash_id_is_in_db = 1;
    /* "run_event, please stop iterating": */
    return 1;
}

static char *do_log(char *log_line, void *param)
{
    VERB1 log("%s", log_line);
    //update_client("%s", log_line);
    return log_line;
}

mw_result_t SaveDebugDump(const char *dump_dir_name,
                          map_crash_data_t& pCrashData)
{
    mw_result_t res;

    if (is_debug_dump_saved(dump_dir_name))
        return MW_IN_DB;

    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        return MW_ERROR;
    struct cdump_state state = { NULL, NULL, false }; /* uid, crash_id, crash_id_is_in_db */
    state.uid = dd_load_text(dd, CD_UID);
    char *time = dd_load_text(dd, FILENAME_TIME);
    char *analyzer = dd_load_text(dd, FILENAME_ANALYZER);
    dd_close(dd);

    res = MW_ERROR;

    struct run_event_state *run_state = new_run_event_state();
    run_state->post_run_callback = is_crash_id_in_db;
    run_state->post_run_param = &state;
    run_state->logging_callback = do_log;
    int r = run_event(run_state, dump_dir_name, "post-create");
    free_run_event_state(run_state);

    /* Is crash id in db? (In this case, is_crash_id_in_db() should have
     * aborted "post-create" event processing as soon as it saw uuid
     * such that uid:uuid (=crash_id) is in database, and set
     * the state.crash_id_is_in_db flag)
     */
    if (!state.crash_id_is_in_db)
    {
        /* No. Was there error on one of processing steps in run_event? */
        if (r != 0)
            goto ret; /* yes */

        /* Was uuid created after all? (In this case, is_crash_id_in_db()
         * should have fetched it and created state.crash_id)
         */
        if (!state.crash_id)
        {
            /* no */
            log("Dump directory '%s' has no UUID element", dump_dir_name);
            goto ret;
        }
    }

    /* Loads pCrashData (from the *first debugdump dir* if this one is a dup)
     * Returns:
     * MW_REPORTED: "the crash is flagged as reported in DB" (which also means it's a dup)
     * MW_OCCURRED: "crash count is != 1" (iow: it is > 1 - dup)
     * MW_OK: "crash count is 1" (iow: this is a new crash, not a dup)
     * else: an error code
     */
    res = SaveDebugDumpToDatabase(state.crash_id,
                    analyzer_has_InformAllUsers(analyzer),
                    time,
                    dump_dir_name,
                    pCrashData);
 ret:
    free(state.crash_id);
    free(state.uid);
    free(time);
    free(analyzer);

    return res;
}

mw_result_t FillCrashInfo(const char *crash_id,
                          map_crash_data_t& pCrashData)
{
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
    database->Connect();
    struct db_row *row = database->GetRow(crash_id);
    database->DisConnect();

    if (!row)
        return MW_ERROR;

    struct dump_dir *dd = dd_opendir(row->db_dump_dir, /*flags:*/ 0);
    if (!dd)
    {
        db_row_free(row);
        return MW_ERROR;
    }

    load_crash_data_from_debug_dump(dd, pCrashData);
    char *events = list_possible_events(dd, NULL, "");
    dd_close(dd);

    add_to_crash_data_ext(pCrashData, CD_EVENTS, CD_SYS, CD_ISNOTEDITABLE, events);
    free(events);

//TODO: we _never_ use CD_SYS, perhaps we should use it here?
    add_to_crash_data(pCrashData, CD_UID              , row->db_uid        );
    add_to_crash_data(pCrashData, CD_UUID             , row->db_uuid       );
    add_to_crash_data(pCrashData, CD_INFORMALL        , row->db_inform_all );
    add_to_crash_data(pCrashData, CD_COUNT            , row->db_count      );
    add_to_crash_data(pCrashData, CD_REPORTED         , row->db_reported   );
    add_to_crash_data(pCrashData, CD_MESSAGE          , row->db_message    );
    add_to_crash_data(pCrashData, CD_DUMPDIR          , row->db_dump_dir   );
    add_to_crash_data(pCrashData, FILENAME_TIME       , row->db_time       );

    db_row_free(row);

    return MW_OK;
}

void GetUUIDsOfCrash(long caller_uid, vector_string_t &result)
{
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
    database->Connect();
    GList *rows = database->GetUIDData(caller_uid);
    database->DisConnect();

    for (GList *li = rows; li != NULL; li = g_list_next(li))
    {
        struct db_row *row = (struct db_row*)li->data;
        string crash_id = ssprintf("%s:%s", row->db_uid, row->db_uuid);
        result.push_back(crash_id);
    }

    // TODO: return GList
    db_list_free(rows);
}

void AddAnalyzerActionOrReporter(const char *pAnalyzer,
                                              const char *pAnalyzerOrReporter,
                                              const char *pArgs)
{
    s_mapAnalyzerActionsAndReporters[pAnalyzer].push_back(make_pair(std::string(pAnalyzerOrReporter), std::string(pArgs)));
}

void AddActionOrReporter(const char *pActionOrReporter,
                                      const char *pArgs)
{
    VERB3 log("AddActionOrReporter('%s','%s')", pActionOrReporter, pArgs);
    s_vectorActionsAndReporters.push_back(make_pair(std::string(pActionOrReporter), std::string(pArgs)));
}
