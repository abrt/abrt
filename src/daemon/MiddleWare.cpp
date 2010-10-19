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
#include <algorithm>  /* for std::find */
#include "abrtlib.h"
#include "Daemon.h"
#include "Settings.h"
#include "rpm.h"
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


static void RunAnalyzerActions(const char *pAnalyzer, const char* pPackageName, const char *pDebugDumpDir, int force);


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

/**
 * Get a local UUID from particular analyzer plugin.
 * @param pAnalyzer A name of an analyzer plugin.
 * @param pDebugDumpDir A debugdump dir containing all necessary data.
 * @return A local UUID.
 */
static std::string GetLocalUUID(const char *pAnalyzer, const char *pDebugDumpDir)
{
    CAnalyzer* analyzer = g_pPluginManager->GetAnalyzer(pAnalyzer);
    if (analyzer)
    {
        return analyzer->GetLocalUUID(pDebugDumpDir);
    }
    throw CABRTException(EXCEP_PLUGIN, "Error running '%s'", pAnalyzer);
}

/**
 * Get a global UUID from particular analyzer plugin.
 * @param pAnalyzer A name of an analyzer plugin.
 * @param pDebugDumpDir A debugdump dir containing all necessary data.
 * @return A global UUID.
 */
static std::string GetGlobalUUID(const char *pAnalyzer,
                                       const char *pDebugDumpDir)
{
    CAnalyzer* analyzer = g_pPluginManager->GetAnalyzer(pAnalyzer);
    if (analyzer)
    {
        return analyzer->GetGlobalUUID(pDebugDumpDir);
    }
    throw CABRTException(EXCEP_PLUGIN, "Error running '%s'", pAnalyzer);
}

/**
 * Take care of getting all additional data needed
 * for computing UUIDs and creating a report for particular analyzer
 * plugin. This report could be send somewhere afterwards.
 * @param pAnalyzer A name of an analyzer plugin.
 * @param pDebugDumpPath A debugdump dir containing all necessary data.
 */
static void run_analyser_CreateReport(const char *pAnalyzer,
                const char *pDebugDumpDir,
                int force)
{
    CAnalyzer* analyzer = g_pPluginManager->GetAnalyzer(pAnalyzer);
    if (analyzer)
    {
        analyzer->CreateReport(pDebugDumpDir, force);
    }
    /* else: GetAnalyzer() already complained, no need to handle it here */
}

/*
 * Called in three cases:
 * (1) by StartJob dbus call -> CreateReportThread(), in the thread
 * (2) by CreateReport dbus call
 * (3) by daemon if AutoReportUID is set for this user's crashes
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

    char caller_uid_str[sizeof(long) * 3 + 2];
    sprintf(caller_uid_str, "%li", caller_uid);

    if (caller_uid != 0 /* not called by root */
     && row->db_inform_all[0] != '1'
     && strcmp(caller_uid_str, row->db_uid) != 0
    ) {
        error_msg("crash '%s' can't be accessed by user with uid %ld", crash_id, caller_uid);
        db_row_free(row);
        return MW_IN_DB_ERROR;
    }

    mw_result_t r = MW_OK;
    try
    {
        struct dump_dir *dd = dd_opendir(row->db_dump_dir, /*flags:*/ 0);
        if (!dd)
        {
            db_row_free(row);
            return MW_ERROR;
        }

        load_crash_data_from_debug_dump(dd, pCrashData);
        dd_close(dd);

        std::string analyzer = get_crash_data_item_content(pCrashData, FILENAME_ANALYZER);
        const char* package = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_PACKAGE);
        char* package_name = get_package_name_from_NVR_or_NULL(package);

        // TODO: explain what run_analyser_CreateReport and RunAnalyzerActions are expected to do.
        // Do they potentially add more files to dump dir?
        // Why we calculate dup_hash after run_analyser_CreateReport but before RunAnalyzerActions?
        // Why do we reload dump dir's data via DebugDumpToCrashReport?

        VERB3 log(" run_analyser_CreateReport('%s')", analyzer.c_str());
        run_analyser_CreateReport(analyzer.c_str(), row->db_dump_dir, force);

        std::string dup_hash = GetGlobalUUID(analyzer.c_str(), row->db_dump_dir);
        VERB3 log(" DUPHASH:'%s'", dup_hash.c_str());

        VERB3 log(" RunAnalyzerActions('%s','%s','%s',force=%d)", analyzer.c_str(), package_name, row->db_dump_dir, force);
        RunAnalyzerActions(analyzer.c_str(), package_name, row->db_dump_dir, force);
        free(package_name);
        if (DebugDumpToCrashReport(row->db_dump_dir, pCrashData))
        {
            add_to_crash_data_ext(pCrashData, CD_UUID   , CD_SYS, CD_ISNOTEDITABLE, row->db_uuid);
            add_to_crash_data_ext(pCrashData, FILENAME_DUPHASH, CD_TXT, CD_ISNOTEDITABLE, dup_hash.c_str());
        }
        else
        {
            db_row_free(row);
            error_msg("Error loading crash data");
            return MW_ERROR;
        }
        db_row_free(row);
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


// Do not trust client_report here!
// dbus handler passes it from user without checking
report_status_t Report(const map_crash_data_t& client_report,
                       const vector_string_t &reporters,
                       map_map_string_t& settings,
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

    const std::string& pDumpDir = get_crash_data_item_content(stored_report, CD_DUMPDIR);

    // Save comment, "how to reproduce", backtrace
//TODO: we should iterate through stored_report and modify all
//modifiable fields which have new data in client_report
    const char *comment = get_crash_data_item_content_or_NULL(client_report, FILENAME_COMMENT);
    const char *reproduce = get_crash_data_item_content_or_NULL(client_report, FILENAME_REPRODUCE);
    const char *backtrace = get_crash_data_item_content_or_NULL(client_report, FILENAME_BACKTRACE);
    if (comment || reproduce || backtrace)
    {
        struct dump_dir *dd = dd_opendir(pDumpDir.c_str(), /*flags:*/ 0);
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

    const std::string& analyzer = get_crash_data_item_content(stored_report, FILENAME_ANALYZER);

    std::string dup_hash = GetGlobalUUID(analyzer.c_str(), pDumpDir.c_str());
    VERB3 log(" DUPHASH:'%s'", dup_hash.c_str());
    add_to_crash_data_ext(stored_report, FILENAME_DUPHASH, CD_TXT, CD_ISNOTEDITABLE, dup_hash.c_str());

    // Run reporters

    VERB3 {
        log("Run reporters");
        log_map_crash_data(client_report, " client_report");
        log_map_crash_data(stored_report, " stored_report");
    }
#define client_report client_report_must_not_be_used_below

    map_crash_data_t::const_iterator its_PACKAGE = stored_report.find(FILENAME_PACKAGE);
    std::string packageNVR = its_PACKAGE->second[CD_CONTENT];
    char * packageName = get_package_name_from_NVR_or_NULL(packageNVR.c_str());

    // analyzer with package name (CCpp:xorg-x11-app) has higher priority
    char* key = xasprintf("%s:%s",analyzer.c_str(),packageName);
    free(packageName);
    map_analyzer_actions_and_reporters_t::iterator end = s_mapAnalyzerActionsAndReporters.end();
    map_analyzer_actions_and_reporters_t::iterator keyPtr = s_mapAnalyzerActionsAndReporters.find(key);
    if (keyPtr == end)
    {
        VERB3 log("'%s' not found, looking for '%s'", key, analyzer.c_str());
        // if there is no such settings, then try default analyzer
        keyPtr = s_mapAnalyzerActionsAndReporters.find(analyzer);
    }
    free(key);

    bool at_least_one_reporter_succeeded = false;
    report_status_t ret;
    std::string message;
    if (keyPtr != end)
    {
        VERB2 log("Found AnalyzerActionsAndReporters for '%s'", analyzer.c_str());

        vector_pair_string_string_t::iterator it_r = keyPtr->second.begin();
        for (; it_r != keyPtr->second.end(); it_r++)
        {
            const char *plugin_name = it_r->first.c_str();

            /* Check if the reporter is in the input list of allowed reporters. */
            if (reporters.end() == std::find(reporters.begin(), reporters.end(), plugin_name))
            {
                continue;
            }

            try
            {
                if (g_pPluginManager->GetPluginType(plugin_name) == REPORTER)
                {
                    CReporter* reporter = g_pPluginManager->GetReporter(plugin_name); /* can't be NULL */
                    map_plugin_settings_t plugin_settings = settings[plugin_name];
                    std::string res = reporter->Report(stored_report, plugin_settings, it_r->second.c_str());
                    ret[plugin_name].push_back("1"); // REPORT_STATUS_IDX_FLAG
                    ret[plugin_name].push_back(res); // REPORT_STATUS_IDX_MSG
                    if (message != "")
                        message += ";";
                    message += res;
                    at_least_one_reporter_succeeded = true;
                }
            }
            catch (CABRTException& e)
            {
                ret[plugin_name].push_back("0");      // REPORT_STATUS_IDX_FLAG
                ret[plugin_name].push_back(e.what()); // REPORT_STATUS_IDX_MSG
                update_client("Reporting via '%s' was not successful: %s", plugin_name, e.what());
            }
        } // for
    } // if

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
 * Check whether particular debugdump directory is saved
 * in database. This check is done together with an UID of an user.
 * @param uid
 *  An UID of an user.
 * @param debug_dump_dir
 *  A debugdump dir containing all necessary data.
 * @return
 *  It returns true if debugdump dir is already saved, otherwise
 *  it returns false.
 * @todo
 *  Use database query instead of dumping all rows and searching in them.
 */
static bool is_debug_dump_saved(long uid, const char *debug_dump_dir)
{
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
    database->Connect();
    GList *table = database->GetUIDData(uid);
    database->DisConnect();

    bool found = false;
    for (GList *li = table; li != NULL; li = g_list_next(li))
    {
        struct db_row *row = (struct db_row*)li->data;
        if (0 == strcmp(row->db_dump_dir, debug_dump_dir))
        {
            found = true;
            break;
        }
    }

    db_list_free(table);

    return found;
}

/**
 * Get a package name from executable name and save
 * package description to particular debugdump directory of a crash.
 * @param pExecutable A name of crashed application.
 * @param pDebugDumpDir A debugdump dir containing all necessary data.
 * @return It return results of operation. See mw_result_t.
 */
static mw_result_t SavePackageDescriptionToDebugDump(const char *pDebugDumpDir)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        perror_msg("fork");
        return MW_ERROR;
    }
    if (pid == 0) /* child */
    {
        char *argv[5];  /* abrt-action-save-package-data [-s] -d DIR NULL */
        char **pp = argv;
        *pp++ = (char*)"abrt-action-save-package-data";
        if (logmode & LOGMODE_SYSLOG)
            *pp++ = (char*)"-s";
        *pp++ = (char*)"-d";
        *pp++ = (char*)pDebugDumpDir;
        *pp = NULL;

        execvp(argv[0], argv);
        perror_msg_and_die("Can't execute '%s'", argv[0]);
    }
    /* parent */
    int status;
    waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? MW_OK : MW_ERROR;
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

bool analyzer_has_AutoReportUIDs(const char *analyzer_name, const char *uid_str)
{
    CAnalyzer* analyzer = g_pPluginManager->GetAnalyzer(analyzer_name);
    if (!analyzer)
    {
        return false;
    }
    map_plugin_settings_t settings = analyzer->GetSettings();
    map_plugin_settings_t::const_iterator it = settings.find("AutoReportUIDs");
    if (it == settings.end())
        return false;

    vector_string_t logins;
    parse_args(it->second.c_str(), logins);

    uid_t uid = xatoi_u(uid_str);
    unsigned size = logins.size();
    for (unsigned ii = 0; ii < size; ii++)
    {
        struct passwd* pw = getpwnam(logins[ii].c_str());
        if (!pw)
            continue;
        if (pw->pw_uid == uid)
            return true;
    }

    return false;
}

void autoreport(const pair_string_string_t& reporter_options, const map_crash_data_t& crash_report)
{
    CReporter* reporter = g_pPluginManager->GetReporter(reporter_options.first.c_str());
    if (!reporter)
    {
        return;
    }
    map_plugin_settings_t plugin_settings;
    /*std::string res =*/ reporter->Report(crash_report, plugin_settings, reporter_options.second.c_str());
}

/**
 * Execute all action plugins, which are associated to
 * particular analyzer plugin.
 * @param pAnalyzer A name of an analyzer plugin.
 * @param pDebugDumpPath A debugdump dir containing all necessary data.
 */
static void RunAnalyzerActions(const char *pAnalyzer, const char *pPackageName, const char *pDebugDumpDir, int force)
{
    map_analyzer_actions_and_reporters_t::iterator analyzer;
    if (pPackageName != NULL)
    {
        /*try to find analyzer:component first*/
        char *analyzer_component = xasprintf("%s:%s", pAnalyzer, pPackageName);
        analyzer = s_mapAnalyzerActionsAndReporters.find(analyzer_component);
        /* if we didn't find an action for specific package, use the generic one */
        if (analyzer == s_mapAnalyzerActionsAndReporters.end())
        {
            VERB2 log("didn't find action for %s, trying just %s", analyzer_component, pAnalyzer);
            map_analyzer_actions_and_reporters_t::iterator analyzer = s_mapAnalyzerActionsAndReporters.find(pAnalyzer);
        }
        free(analyzer_component);
    }
    else
    {
        VERB2 log("no package name specified, trying to find action for: %s", pAnalyzer);
        analyzer = s_mapAnalyzerActionsAndReporters.find(pAnalyzer);
    }
    if (analyzer != s_mapAnalyzerActionsAndReporters.end())
    {
        vector_pair_string_string_t::iterator it_a = analyzer->second.begin();
        for (; it_a != analyzer->second.end(); it_a++)
        {
            const char *plugin_name = it_a->first.c_str();
            CAction* action = g_pPluginManager->GetAction(plugin_name, /*silent:*/ true);
            if (!action)
            {
                /* GetAction() already complained if no such plugin.
                 * If plugin exists but isn't an Action, it's not an error.
                 */
                continue;
            }
            try
            {
                action->Run(pDebugDumpDir, it_a->second.c_str(), force);
            }
            catch (CABRTException& e)
            {
                update_client("Action performed by '%s' was not successful: %s", plugin_name, e.what());
            }
        }
    }
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

mw_result_t SaveDebugDump(const char *pDebugDumpDir,
                          map_crash_data_t& pCrashData)
{
    mw_result_t res;

    struct dump_dir *dd = dd_opendir(pDebugDumpDir, /*flags:*/ 0);
    if (!dd)
        return MW_ERROR;

    char *time = dd_load_text(dd, FILENAME_TIME);
    char *uid = dd_load_text(dd, CD_UID);
    char *analyzer = dd_load_text(dd, FILENAME_ANALYZER);

    dd_close(dd);

    /* Convert UID string to number uid_num. The UID string can be modified by user or
       wrongly saved (empty or non-numeric), so xatou() cannot be used here,
       because it would kill the daemon. */
    char *endptr;
    errno = 0;
    unsigned long uid_num = strtoul(uid, &endptr, 10);
    if (errno || uid == endptr || *endptr != '\0' || uid_num > UINT_MAX)
    {
        error_msg("Invalid UID '%s' loaded from %s", uid, pDebugDumpDir);
        res = MW_ERROR;
        goto error;
    }

    if (is_debug_dump_saved(uid_num, pDebugDumpDir))
    {
        res = MW_IN_DB;
        goto error;
    }

    res = SavePackageDescriptionToDebugDump(pDebugDumpDir);
    if (res != MW_OK)
        goto error;

    {
        std::string UUID = GetLocalUUID((analyzer) ? analyzer : "", pDebugDumpDir);
        std::string crash_id = ssprintf("%s:%s", uid, UUID.c_str());
        /* Loads pCrashData (from the *first debugdump dir* if this one is a dup)
         * Returns:
         * MW_REPORTED: "the crash is flagged as reported in DB" (which also means it's a dup)
         * MW_OCCURRED: "crash count is != 1" (iow: it is > 1 - dup)
         * MW_OK: "crash count is 1" (iow: this is a new crash, not a dup)
         * else: an error code
         */

        res = SaveDebugDumpToDatabase(crash_id.c_str(),
                    analyzer_has_InformAllUsers(analyzer),
                    time,
                    pDebugDumpDir,
                    pCrashData);
    }

error:
    free(time);
    free(uid);
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
    dd_close(dd);

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
