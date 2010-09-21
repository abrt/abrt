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
#include <fnmatch.h>
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


static char* is_text_file(const char *name, ssize_t *sz)
{
    /* We were using magic.h API to check for file being text, but it thinks
     * that file containing just "0" is not text (!!)
     * So, we do it ourself.
     */

    int fd = open(name, O_RDONLY);
    if (fd < 0)
        return NULL; /* it's not text (because it does not exist! :) */

    /* Maybe 64k limit is small. But _some_ limit is necessary:
     * fields declared "text" may end up in editing fields and such.
     * We don't want to accidentally end up with 100meg text in a textbox!
     * So, don't remove this. If you really need to, raise the limit.
     */
    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0 || size > 64*1024)
    {
        close(fd);
        return NULL; /* it's not a SMALL text */
    }
    lseek(fd, 0, SEEK_SET);

    char *buf = (char*)xmalloc(*sz);
    ssize_t r = *sz = full_read(fd, buf, *sz);
    close(fd);
    if (r < 0)
    {
        free(buf);
        return NULL; /* it's not text (because we can't read it) */
    }

    /* Some files in our dump directories are known to always be textual */
    const char *base = strrchr(name, '/');
    if (base)
    {
        base++;
        if (strcmp(base, FILENAME_BACKTRACE) == 0
         || strcmp(base, FILENAME_CMDLINE) == 0
        ) {
            return buf;
        }
    }

    /* Every once in a while, even a text file contains a few garbled
     * or unexpected non-ASCII chars. We should not declare it "binary".
     */
    const unsigned RATIO = 50;
    unsigned total_chars = r + RATIO;
    unsigned bad_chars = 1; /* 1 prevents division by 0 later */
    while (--r >= 0)
    {
        if (buf[r] >= 0x7f
         /* among control chars, only '\t','\n' etc are allowed */
         || (buf[r] < ' ' && !isspace(buf[r]))
        ) {
            if (buf[r] == '\0')
            {
                /* We don't like NULs very much. Not text for sure! */
                free(buf);
                return NULL;
            }
            bad_chars++;
        }
    }

    if ((total_chars / bad_chars) >= RATIO)
        return buf; /* looks like text to me */

    free(buf);
    return NULL; /* it's binary */
}

static void load_crash_data_from_debug_dump(struct dump_dir *dd, map_crash_data_t& data)
{
    char *short_name;
    char *full_name;

    dd_init_next_file(dd);
    while (dd_get_next_file(dd, &short_name, &full_name))
    {
        ssize_t sz = 4*1024;
        char *text = NULL;
        bool editable = is_editable_file(short_name);

        if (!editable)
        {
            text = is_text_file(full_name, &sz);
            if (!text)
            {
                add_to_crash_data_ext(data,
                        short_name,
                        CD_BIN,
                        CD_ISNOTEDITABLE,
                        full_name
                );

                free(short_name);
                free(full_name);
                continue;
            }
        }

        char *content;
        if (sz < 4*1024) /* is_text_file did read entire file */
            content = xstrndup(text, sz); //TODO: can avoid this copying if is_text_file() adds NUL
        else /* no, need to read it all */
            content = dd_load_text(dd, short_name);
        free(text);

        add_to_crash_data_ext(data,
                short_name,
                CD_TXT,
                editable ? CD_ISEDITABLE : CD_ISNOTEDITABLE,
                content
        );
        free(short_name);
        free(full_name);
        free(content);
    }
}

/**
 * Transforms a debugdump directory to inner crash
 * report form. This form is used for later reporting.
 * @param pDebugDumpDir A debugdump dir containing all necessary data.
 * @param pCrashData A created crash report.
 */
static bool DebugDumpToCrashReport(const char *pDebugDumpDir, map_crash_data_t& pCrashData)
{
    VERB3 log(" DebugDumpToCrashReport('%s')", pDebugDumpDir);

    struct dump_dir *dd = dd_init();
    if (dd_opendir(dd, pDebugDumpDir))
    {
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

    dd_close(dd);
    return false;
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

    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
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
        struct dump_dir *dd = dd_init();
        if (!dd_opendir(dd, row->db_dump_dir))
        {
            dd_close(dd);
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
            add_to_crash_data_ext(pCrashData, CD_DUPHASH, CD_TXT, CD_ISNOTEDITABLE, dup_hash.c_str());
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
        struct dump_dir *dd = dd_init();
        if (dd_opendir(dd, pDumpDir.c_str()))
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
        }
        dd_close(dd);
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
    add_to_crash_data_ext(stored_report, CD_DUPHASH, CD_TXT, CD_ISNOTEDITABLE, dup_hash.c_str());

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
        CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
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
    if (g_settings_sDatabase.empty())
        error_msg_and_die(_("Database plugin not specified. Please check abrtd settings."));

    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
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
 * Returns the first full path argument in the command line or NULL.
 * Skips options are in form "-XXX".
 * Caller must delete the returned string using free().
 */
static char *get_argv1_if_full_path(const char* cmdline)
{
    const char *argv1 = strpbrk(cmdline, " \t");
    while (argv1 != NULL)
    {
        /* we found space in cmdline, so it might contain
         * path to some script like:
         * /usr/bin/python [-XXX] /usr/bin/system-control-network
         */
        argv1++; /* skip the space */
        if (*argv1 == '-') /* skip arguments */
        {
            /* looks like -XXX in "perl -XXX /usr/bin/script.pl", skip */
            argv1 = strpbrk(argv1, " \t");
            continue;
        }
        else if (*argv1 == ' ' || *argv1 == '\t') /* skip multiple spaces */
            continue;
        else if (*argv1 != '/')
        {
            /* if the string following the space doesn't start
             * with '/' it's probably not a full path to script
             * and we can't use it to determine the package name
             */
            break;
        }

        /* cut the rest of cmdline arguments */
        int len = strchrnul(argv1, ' ') - argv1;
        return xstrndup(argv1, len);
    }
    return NULL;
}

static bool is_path_blacklisted(const char *path)
{
    set_string_t::iterator it = g_settings_setBlackListedPaths.begin();
    while (it != g_settings_setBlackListedPaths.end())
    {
        if (fnmatch(it->c_str(), path, /*flags:*/ 0) == 0)
        {
            return true;
        }
        it++;
    }
    return false;
}


/**
 * Get a package name from executable name and save
 * package description to particular debugdump directory of a crash.
 * @param pExecutable A name of crashed application.
 * @param pDebugDumpDir A debugdump dir containing all necessary data.
 * @return It return results of operation. See mw_result_t.
 */
static mw_result_t SavePackageDescriptionToDebugDump(
                const char *pExecutable,
                const char *cmdline,
                bool remote,
                const char *pDebugDumpDir)
{
    char* rpm_pkg = NULL;
    char* packageName = NULL;
    char* component = NULL;
    std::string scriptName; /* only if "interpreter /path/to/script" */

    if (strcmp(pExecutable, "kernel") == 0)
    {
        component = xstrdup("kernel");
        rpm_pkg = xstrdup("kernel");
        packageName = xstrdup("kernel");
    }
    else
    {
        if (is_path_blacklisted(pExecutable))
        {
            log("Blacklisted executable '%s'", pExecutable);
            return MW_BLACKLISTED;
        }

        rpm_pkg = rpm_get_package_nvr(pExecutable);
        if (rpm_pkg == NULL)
        {
            if (g_settings_bProcessUnpackaged || remote)
            {
                VERB2 log("Crash in unpackaged executable '%s', proceeding without packaging information", pExecutable);

                struct dump_dir *dd = dd_init();
                if (!dd_opendir(dd, pDebugDumpDir))
                {
                    dd_close(dd);
                    return MW_ERROR;
                }

                dd_save_text(dd, FILENAME_PACKAGE, "");
                dd_save_text(dd, FILENAME_COMPONENT, "");
                dd_save_text(dd, FILENAME_DESCRIPTION, "Crashed executable does not belong to any installed package");

                dd_close(dd);
                return MW_OK;
            }
            else
            {
                log("Executable '%s' doesn't belong to any package", pExecutable);
                return MW_PACKAGE_ERROR;
            }
        }

        /* Check well-known interpreter names */

        const char *basename = strrchr(pExecutable, '/');
        if (basename) basename++; else basename = pExecutable;

        /* Add more interpreters as needed */
        if (strcmp(basename, "python") == 0
         || strcmp(basename, "perl") == 0
        ) {
// TODO: we don't verify that python executable is not modified
// or that python package is properly signed
// (see CheckFingerprint/CheckHash below)

            /* Try to find package for the script by looking at argv[1].
             * This will work only if the cmdline contains the whole path.
             * Example: python /usr/bin/system-control-network
             */
            bool knownOrigin = false;
            char *script_name = get_argv1_if_full_path(cmdline);
            if (script_name)
            {
                char *script_pkg = rpm_get_package_nvr(script_name);
                if (script_pkg)
                {
                    /* There is a well-formed script name in argv[1],
                     * and it does belong to some package.
                     * Replace interpreter's rpm_pkg and pExecutable
                     * with data pertaining to the script.
                     */
                    free(rpm_pkg);
                    rpm_pkg = script_pkg;
                    scriptName = script_name;
                    pExecutable = scriptName.c_str();
                    knownOrigin = true;
                    /* pExecutable has changed, check it again */
                    if (is_path_blacklisted(pExecutable))
                    {
                        log("Blacklisted executable '%s'", pExecutable);
                        return MW_BLACKLISTED;
                    }
                }
                free(script_name);
            }

            if (!knownOrigin && !g_settings_bProcessUnpackaged && !remote)
            {
                log("Interpreter crashed, but no packaged script detected: '%s'", cmdline);
                return MW_PACKAGE_ERROR;
            }
        }

        packageName = get_package_name_from_NVR_or_NULL(rpm_pkg);
        VERB2 log("Package:'%s' short:'%s'", rpm_pkg, packageName);

        if (g_settings_setBlackListedPkgs.find(packageName) != g_settings_setBlackListedPkgs.end())
        {
            log("Blacklisted package '%s'", packageName);
            free(packageName);
            return MW_BLACKLISTED;
        }
        if (g_settings_bOpenGPGCheck && !remote)
        {
            if (rpm_chk_fingerprint(packageName))
            {
                log("Package '%s' isn't signed with proper key", packageName);
                free(packageName);
                return MW_GPG_ERROR;
            }
            /*
              Checking the MD5 sum requires to run prelink to "un-prelink" the
              binaries - this is considered potential security risk so we don't
              use it, until we find some non-intrusive way

              Delete?
            */
            /*
            if (!CheckHash(packageName.c_str(), pExecutable))
            {
                error_msg("Executable '%s' seems to be modified, "
                                "doesn't match one from package '%s'",
                                pExecutable, packageName.c_str());
                return MW_GPG_ERROR;
            }
            */
        }
        component = rpm_get_component(pExecutable);
    }

    char *dsc = rpm_get_description(packageName);
    free(packageName);

    char host[HOST_NAME_MAX + 1];
    if (!remote)
    {
        // HOST_NAME_MAX is defined in limits.h
        int ret = gethostname(host, HOST_NAME_MAX);
        host[HOST_NAME_MAX] = '\0';
        if (ret < 0)
        {
            perror_msg("gethostname");
            host[0] = '\0';
        }
    }

    struct dump_dir *dd = dd_init();
    if (dd_opendir(dd, pDebugDumpDir))
    {
        if (rpm_pkg)
        {
            dd_save_text(dd, FILENAME_PACKAGE, rpm_pkg);
            free(rpm_pkg);
        }

        if (dsc)
        {
            dd_save_text(dd, FILENAME_DESCRIPTION, dsc);
            free(dsc);
        }

        if (component)
        {
            dd_save_text(dd, FILENAME_COMPONENT, component);
            free(component);
        }

        if (!remote)
            dd_save_text(dd, FILENAME_HOSTNAME, host);

        dd_close(dd);
        return MW_OK;
    }
    dd_close(dd);

    return MW_ERROR;
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
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
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
    int remote = 0;

    struct dump_dir *dd = dd_init();
    if (!dd_opendir(dd, pDebugDumpDir))
    {
        dd_close(dd);
        return MW_ERROR;
    }

    char *time = dd_load_text(dd, FILENAME_TIME);
    char *uid = dd_load_text(dd, CD_UID);
    char *analyzer = dd_load_text(dd, FILENAME_ANALYZER);
    char *executable = dd_load_text(dd, FILENAME_EXECUTABLE);
    char *cmdline = dd_load_text(dd, FILENAME_CMDLINE);

    char *remote_str;
    if (dd_exist(dd, FILENAME_REMOTE))
        remote_str = dd_load_text(dd, FILENAME_REMOTE);
    else
        remote_str = xstrdup("");

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

    if (remote_str[0])
        remote = remote_str[0] != '1';

    res = SavePackageDescriptionToDebugDump(executable,
                                            cmdline,
                                            remote,
                                            pDebugDumpDir
    );
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
    free(remote_str);
    free(time);
    free(executable);
    free(cmdline);
    free(uid);
    free(analyzer);

    return res;
}

mw_result_t FillCrashInfo(const char *crash_id,
                          map_crash_data_t& pCrashData)
{
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
    database->Connect();
    struct db_row *row = database->GetRow(crash_id);
    database->DisConnect();

    if (!row)
        return MW_ERROR;

    struct dump_dir *dd = dd_init();
    if (!dd_opendir(dd, row->db_dump_dir))
    {
        dd_close(dd);
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
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
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
