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
#include "abrt_types.h"
#include "Daemon.h"
#include "Settings.h"
#include "RPM.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#include "MiddleWare.h"


/**
 * An instance of CPluginManager. When MiddleWare wants to do something
 * with plugins, it calls the plugin manager.
 * @see PluginManager.h
 */
CPluginManager* g_pPluginManager;
/**
 * A set of blacklisted packages.
 */
set_string_t g_setBlackList;
/**
 * An instance of CRPM used for package checking.
 * @see RPM.h
 */
static CRPM s_RPM;


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


static void RunAnalyzerActions(const char *pAnalyzer, const char *pDebugDumpDir);


static char* is_text_file(const char *name, ssize_t *sz)
{
    /* We were using magic.h API to check for file being text, but it thinks
     * that file containing just "0" is not text (!!)
     * So, we do it ourself.
     */

    int fd = open(name, O_RDONLY);
    if (fd < 0)
        return NULL; /* it's not text (because it does not exist! :) */

    char *buf = (char*)xmalloc(*sz);
    ssize_t r = *sz = full_read(fd, buf, *sz);
    close(fd);
    if (r < 0)
    {
        free(buf);
        return NULL; /* it's not text (because we can't read it) */
    }

    /* Some files in our dump directories are known to always be textual */
    if (strcmp(name, FILENAME_BACKTRACE) == 0
     || strcmp(name, FILENAME_CMDLINE) == 0
    ) {
        return buf;
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

static void load_crash_data_from_debug_dump(CDebugDump& dd, map_crash_data_t& data)
{
    std::string short_name;
    std::string full_name;

    dd.InitGetNextFile();
    while (dd.GetNextFile(&short_name, &full_name))
    {
        ssize_t sz = 4*1024;
        char *text = is_text_file(full_name.c_str(), &sz);
        if (!text)
        {
            add_to_crash_data_ext(data,
                    short_name.c_str(),
                    CD_BIN,
                    CD_ISNOTEDITABLE,
                    full_name.c_str()
            );
            continue;
        }

        std::string content;
        if (sz < 4*1024) /* is_text_file did read entire file */
            content.assign(text, sz);
        else /* no, need to read it all */
            dd.LoadText(short_name.c_str(), content);
        free(text);

        if (short_name == FILENAME_ARCHITECTURE
         || short_name == FILENAME_KERNEL
         || short_name == FILENAME_PACKAGE
         || short_name == FILENAME_COMPONENT
         || short_name == FILENAME_RELEASE
         || short_name == FILENAME_EXECUTABLE
        ) {
            add_to_crash_data_ext(data,
                    short_name.c_str(),
                    CD_TXT,
                    CD_ISNOTEDITABLE,
                    content.c_str()
            );
            continue;
        }

        if (short_name != FILENAME_UID
         && short_name != FILENAME_ANALYZER
         && short_name != FILENAME_TIME
         && short_name != FILENAME_DESCRIPTION
         && short_name != FILENAME_REPRODUCE
         && short_name != FILENAME_COMMENT
        ) {
            add_to_crash_data_ext(
                    data,
                    short_name.c_str(),
                    CD_TXT,
                    CD_ISEDITABLE,
                    content.c_str()
            );
        }
    }
}

/**
 * Transforms a debugdump directory to inner crash
 * report form. This form is used for later reporting.
 * @param pDebugDumpDir A debugdump dir containing all necessary data.
 * @param pCrashData A created crash report.
 */
static void DebugDumpToCrashReport(const char *pDebugDumpDir, map_crash_data_t& pCrashData)
{
    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    if (!dd.Exist(FILENAME_ARCHITECTURE)
     || !dd.Exist(FILENAME_KERNEL)
     || !dd.Exist(FILENAME_PACKAGE)
     || !dd.Exist(FILENAME_COMPONENT)
     || !dd.Exist(FILENAME_RELEASE)
     || !dd.Exist(FILENAME_EXECUTABLE)
    ) {
        throw CABRTException(EXCEP_ERROR, "DebugDumpToCrashReport(): One or more of important file(s) are missing");
    }

    load_crash_data_from_debug_dump(dd, pCrashData);
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
static void CreateReport(const char *pAnalyzer,
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

mw_result_t CreateCrashReport(const char *pUUID,
                const char *pUID,
                int force,
                map_crash_data_t& pCrashData)
{
    VERB2 log("CreateCrashReport('%s','%s',result)", pUUID, pUID);

    database_row_t row;
    if (pUUID[0] != '\0')
    {
        CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
        database->Connect();
        row = database->GetRow(pUUID, pUID);
        database->DisConnect();
    }
    if (pUUID[0] == '\0' || row.m_sUUID != pUUID)
    {
        error_msg("UUID '%s' is not in database", pUUID);
        return MW_IN_DB_ERROR;
    }

    try
    {
        CDebugDump dd;
        std::string analyzer;
        std::string comment;
        std::string reproduce = "1.\n2.\n3.\n";

        VERB3 log(" LoadText(FILENAME_ANALYZER,'%s')", row.m_sDebugDumpDir.c_str());
        dd.Open(row.m_sDebugDumpDir.c_str());
        dd.LoadText(FILENAME_ANALYZER, analyzer);
        if (dd.Exist(FILENAME_COMMENT))
        {
            dd.LoadText(FILENAME_COMMENT, comment);
        }
        if (dd.Exist(FILENAME_REPRODUCE))
        {
            dd.LoadText(FILENAME_REPRODUCE, reproduce);
        }
        dd.Close();

        VERB3 log(" CreateReport('%s')", analyzer.c_str());
        CreateReport(analyzer.c_str(), row.m_sDebugDumpDir.c_str(), force);

        std::string gUUID = GetGlobalUUID(analyzer.c_str(), row.m_sDebugDumpDir.c_str());
        VERB3 log(" GetGlobalUUID:'%s'", gUUID.c_str());

        VERB3 log(" RunAnalyzerActions('%s','%s')", analyzer.c_str(), row.m_sDebugDumpDir.c_str());
        RunAnalyzerActions(analyzer.c_str(), row.m_sDebugDumpDir.c_str());
        VERB3 log(" DebugDumpToCrashReport");
        DebugDumpToCrashReport(row.m_sDebugDumpDir.c_str(), pCrashData);

        add_to_crash_data_ext(pCrashData, CD_UUID      , CD_TXT, CD_ISNOTEDITABLE, gUUID.c_str()    );
        add_to_crash_data_ext(pCrashData, CD_MWANALYZER, CD_SYS, CD_ISNOTEDITABLE, analyzer.c_str() );
        add_to_crash_data_ext(pCrashData, CD_MWUID     , CD_SYS, CD_ISNOTEDITABLE, pUID             );
        add_to_crash_data_ext(pCrashData, CD_MWUUID    , CD_SYS, CD_ISNOTEDITABLE, pUUID            );
        add_to_crash_data_ext(pCrashData, CD_COMMENT   , CD_TXT, CD_ISEDITABLE   , comment.c_str()  );
        add_to_crash_data_ext(pCrashData, CD_REPRODUCE , CD_TXT, CD_ISEDITABLE   , reproduce.c_str());
    }
    catch (CABRTException& e)
    {
        error_msg("%s", e.what());
        if (e.type() == EXCEP_DD_OPEN)
        {
            return MW_ERROR;
        }
        if (e.type() == EXCEP_DD_LOAD)
        {
            return MW_FILE_ERROR;
        }
        if (e.type() == EXCEP_PLUGIN)
        {
            return MW_PLUGIN_ERROR;
        }
        return MW_CORRUPTED;
    }

    return MW_OK;
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
        action->Run(pActionDir, pPluginArgs);
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
                DebugDumpToCrashReport(pDebugDumpDir, crashReport);
                VERB2 log("%s.Report(...)", plugin_name);
                reporter->Report(crashReport, plugin_settings, it_ar->second.c_str());
            }
            else if (tp == ACTION)
            {
                CAction* action = g_pPluginManager->GetAction(plugin_name); /* can't be NULL */
                VERB2 log("%s.Run('%s','%s')", plugin_name, pDebugDumpDir, it_ar->second.c_str());
                action->Run(pDebugDumpDir, it_ar->second.c_str());
            }
        }
        catch (CABRTException& e)
        {
            error_msg("Activation of plugin '%s' was not successful: %s", plugin_name, e.what());
        }
    }
}


static bool CheckReport(const map_crash_data_t& pCrashData)
{
    map_crash_data_t::const_iterator it_analyzer = pCrashData.find(CD_MWANALYZER);
    map_crash_data_t::const_iterator it_mwuid = pCrashData.find(CD_MWUID);
    map_crash_data_t::const_iterator it_mwuuid = pCrashData.find(CD_MWUUID);

    map_crash_data_t::const_iterator it_package = pCrashData.find(FILENAME_PACKAGE);
    map_crash_data_t::const_iterator it_architecture = pCrashData.find(FILENAME_ARCHITECTURE);
    map_crash_data_t::const_iterator it_kernel = pCrashData.find(FILENAME_KERNEL);
    map_crash_data_t::const_iterator it_component = pCrashData.find(FILENAME_COMPONENT);
    map_crash_data_t::const_iterator it_release = pCrashData.find(FILENAME_RELEASE);
    map_crash_data_t::const_iterator it_executable = pCrashData.find(FILENAME_EXECUTABLE);

    map_crash_data_t::const_iterator end = pCrashData.end();

    if (it_package == end)
    {
        return false;
    }

    // FIXME: bypass the test if it's kerneloops
    if (it_package->second[CD_CONTENT] == "kernel")
        return true;

    if (it_analyzer == end || it_mwuid == end ||
        it_mwuuid == end || /* it_package == end || */
        it_architecture == end || it_kernel == end ||
        it_component == end || it_release == end ||
        it_executable == end)
    {
        return false;
    }

    if (it_analyzer->second[CD_CONTENT] == "" || it_mwuid->second[CD_CONTENT] == "" ||
        it_mwuuid->second[CD_CONTENT] == "" || it_package->second[CD_CONTENT] == "" ||
        it_architecture->second[CD_CONTENT] == "" || it_kernel->second[CD_CONTENT] == "" ||
        it_component->second[CD_CONTENT] == "" || it_release->second[CD_CONTENT] == "" ||
        it_executable->second[CD_CONTENT] == "")
    {
        return false;
    }

    return true;
}

report_status_t Report(const map_crash_data_t& pCrashData,
                       map_map_string_t& pSettings,
                       const char *pUID)
{
    report_status_t ret;

    /* dbus handler passes pCrashData from user without checking it */

    if (!CheckReport(pCrashData))
    {
        throw CABRTException(EXCEP_ERROR, "Report(): Some of mandatory report data are missing.");
    }

    const std::string& analyzer    = get_crash_data_item_content(pCrashData, CD_MWANALYZER);
    const std::string& UID         = get_crash_data_item_content(pCrashData, CD_MWUID);
    const std::string& UUID        = get_crash_data_item_content(pCrashData, CD_MWUUID);
    const std::string& packageNVR  = get_crash_data_item_content(pCrashData, FILENAME_PACKAGE);
    std::string packageName = packageNVR.substr(0, packageNVR.rfind("-", packageNVR.rfind("-") - 1));

    // Save comment and "how to reproduce"
    map_crash_data_t::const_iterator it_comment = pCrashData.find(CD_COMMENT);
    map_crash_data_t::const_iterator it_reproduce = pCrashData.find(CD_REPRODUCE);
    if (it_comment != pCrashData.end() || it_reproduce != pCrashData.end())
    {
        std::string pDumpDir = getDebugDumpDir(UUID.c_str(), UID.c_str());
        CDebugDump dd;
        dd.Open(pDumpDir.c_str());
        if (it_comment != pCrashData.end())
        {
            dd.SaveText(FILENAME_COMMENT, it_comment->second[CD_CONTENT].c_str());
        }
        if (it_reproduce != pCrashData.end())
        {
            dd.SaveText(FILENAME_REPRODUCE, it_reproduce->second[CD_CONTENT].c_str());
        }
    }

    // analyzer with package name (CCpp:xorg-x11-app) has higher priority
    std::string key = analyzer + ":" + packageName;
    map_analyzer_actions_and_reporters_t::iterator end = s_mapAnalyzerActionsAndReporters.end();
    map_analyzer_actions_and_reporters_t::iterator keyPtr = s_mapAnalyzerActionsAndReporters.find(key);
    if (keyPtr == end)
    {
        // if there is no such settings, then try default analyzer
        keyPtr = s_mapAnalyzerActionsAndReporters.find(analyzer);
        key = analyzer;
    }

    std::string message;
    if (keyPtr != end)
    {
        VERB2 log("Found AnalyzerActionsAndReporters for '%s'", key.c_str());

        vector_pair_string_string_t::iterator it_r = keyPtr->second.begin();
        for (; it_r != keyPtr->second.end(); it_r++)
        {
            const char *plugin_name = it_r->first.c_str();
            try
            {
                if (g_pPluginManager->GetPluginType(plugin_name) == REPORTER)
                {
                    CReporter* reporter = g_pPluginManager->GetReporter(plugin_name); /* can't be NULL */
#if 0 /* Using ~user/.abrt/ is bad wrt security */
                    std::string home;
                    map_plugin_settings_t oldSettings;
                    map_plugin_settings_t newSettings;

                    if (pUID != "")
                    {
                        home = get_home_dir(xatoi_u(pUID.c_str()));
                        if (home != "")
                        {
                            oldSettings = reporter->GetSettings();

                            if (LoadPluginSettings(home + "/.abrt/" + plugin_name + "."PLUGINS_CONF_EXTENSION, newSettings))
                            {
                                reporter->SetSettings(newSettings);
                            }
                        }
                    }
#endif
                    map_plugin_settings_t plugin_settings = pSettings[plugin_name];
                    std::string res = reporter->Report(pCrashData, plugin_settings, it_r->second.c_str());

#if 0 /* Using ~user/.abrt/ is bad wrt security */
                    if (home != "")
                    {
                        reporter->SetSettings(oldSettings);
                    }
#endif
                    ret[plugin_name].push_back("1"); // REPORT_STATUS_IDX_FLAG
                    ret[plugin_name].push_back(res); // REPORT_STATUS_IDX_MSG
                    if (message != "")
                        message += "; ";
                    message += res;
                }
            }
            catch (CABRTException& e)
            {
                ret[plugin_name].push_back("0");      // REPORT_STATUS_IDX_FLAG
                ret[plugin_name].push_back(e.what()); // REPORT_STATUS_IDX_MSG
                update_client("Reporting via '%s' was not successful: %s", plugin_name, e.what());
            }
        }
    }

    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
    database->Connect();
    database->SetReported(UUID.c_str(), UID.c_str(), message.c_str());
    database->DisConnect();

    return ret;
}

/**
 * Check whether particular debugdump directory is saved
 * in database. This check is done together with an UID of an user.
 * @param pUID an UID of an user.
 * @param pDebugDumpDir A debugdump dir containing all necessary data.
 * @return It returns true if debugdump dir is already saved, otherwise
 * it returns false.
 */
static bool IsDebugDumpSaved(const char *pUID,
                                   const char *pDebugDumpDir)
{
    /* TODO: use database query instead of dumping all rows and searching in them */

    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
    database->Connect();
    vector_database_rows_t rows = database->GetUIDData(pUID);
    database->DisConnect();

    int ii;
    bool found = false;
    for (ii = 0; ii < rows.size(); ii++)
    {
        if (rows[ii].m_sDebugDumpDir == pDebugDumpDir)
        {
            found = true;
            break;
        }
    }

    return found;
}

void LoadOpenGPGPublicKey(const char* key)
{
    VERB1 log("Loading GPG key '%s'", key);
    s_RPM.LoadOpenGPGPublicKey(key);
}

/**
 * Get a package name from executable name and save
 * package description to particular debugdump directory of a crash.
 * @param pExecutable A name of crashed application.
 * @param pDebugDumpDir A debugdump dir containing all necessary data.
 * @return It return results of operation. See mw_result_t.
 */
static char *get_argv1_if_full_path(const char* cmdline)
{
    char *argv1 = (char*) strchr(cmdline, ' ');
    if (argv1 != NULL)
    {
        /* we found space in cmdline, so it might contain
         * path to some script like:
         * /usr/bin/python /usr/bin/system-control-network
         */
        argv1++;
        /* if the string following the space doesn't start
         * with '/' it's probably not a full path to script
         * and we can't use it to determine the package name
         */
        if (*argv1 != '/')
        {
            return NULL;
        }
        int len = strchrnul(argv1, ' ') - argv1;
        /* cut the cmdline arguments */
        argv1 = xstrndup(argv1, len);
    }
    return argv1;
}
static mw_result_t SavePackageDescriptionToDebugDump(
                const char *pExecutable,
                const char *cmdline,
                const char *pDebugDumpDir)
{
    std::string package;
    std::string packageName;
    std::string scriptName; /* only if "interpreter /path/to/script" */

    if (strcmp(pExecutable, "kernel") == 0)
    {
        packageName = package = "kernel";
    }
    else
    {
        char *rpm_pkg = GetPackage(pExecutable);
        if (rpm_pkg == NULL)
        {
            log("Executable '%s' doesn't belong to any package", pExecutable);
            return MW_PACKAGE_ERROR;
        }

        /* Check well-known interpreter names */

        const char *basename = strrchr(pExecutable, '/');
        if (basename) basename++; else basename = pExecutable;

        /* Add "perl" and such as needed */
        if (strcmp(basename, "python") == 0)
        {
// TODO: we don't verify that python executable is not modified
// or that python package is properly signed
// (see CheckFingerprint/CheckHash below)

            /* Try to find package for the script by looking at argv[1].
             * This will work only of the cmdline contains the whole path.
             * Example: python /usr/bin/system-control-network
             */
            char *script_name = get_argv1_if_full_path(cmdline);
            if (script_name)
            {
                char *script_pkg = GetPackage(script_name);
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
                }
                free(script_name);
            }
        }

        package = rpm_pkg;
        packageName = package.substr(0, package.rfind("-", package.rfind("-") - 1));
        VERB2 log("Package:'%s' short:'%s'", rpm_pkg, packageName.c_str());
        free(rpm_pkg);

	if (g_setBlackList.find(packageName) != g_setBlackList.end())
        {
            log("Blacklisted package '%s'", packageName.c_str());
            return MW_BLACKLISTED;
        }
        if (g_settings_bOpenGPGCheck)
        {
            if (!s_RPM.CheckFingerprint(packageName.c_str()))
            {
                log("Package '%s' isn't signed with proper key", packageName.c_str());
                return MW_GPG_ERROR;
            }
            if (!CheckHash(packageName.c_str(), pExecutable))
            {
                error_msg("Executable '%s' seems to be modified, "
                                "doesn't match one from package '%s'",
                                pExecutable, packageName.c_str());
                return MW_GPG_ERROR;
            }
        }
    }

    std::string description = GetDescription(packageName.c_str());
    std::string component = GetComponent(pExecutable);
    try
    {
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.SaveText(FILENAME_PACKAGE, package.c_str());
        dd.SaveText(FILENAME_DESCRIPTION, description.c_str());
        dd.SaveText(FILENAME_COMPONENT, component.c_str());
    }
    catch (CABRTException& e)
    {
        error_msg("%s", e.what());
        if (e.type() == EXCEP_DD_SAVE)
        {
            return MW_FILE_ERROR;
        }
        return MW_ERROR;
    }

    return MW_OK;
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

bool analyzer_has_AutoReportUIDs(const char *analyzer_name, const char* uid)
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

    if ((strcmp(analyzer_name, "Kerneloops") == 0) && (strcmp(uid, "-1") == 0))
        return true;

/*
    vector_string_t logins;
    parse_args(it->second.c_str(), logins);

    unsigned size = logins.size();
    if (size == 0)
        return false;

    for (unsigned ii = 0; ii < size; ii++)
    {
        uid_t id = getuidbyname(logins[ii].c_str())
        if (id == (uid_t)-1)
            continue;
        if (strcmp(uid, to_string(id).c_str()) == 0)
            return true;
    }
*/
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
static void RunAnalyzerActions(const char *pAnalyzer, const char *pDebugDumpDir)
{
    map_analyzer_actions_and_reporters_t::iterator analyzer = s_mapAnalyzerActionsAndReporters.find(pAnalyzer);
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
                action->Run(pDebugDumpDir, it_a->second.c_str());
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
static mw_result_t SaveDebugDumpToDatabase(const char *pUUID,
                const char *pUID,
                const char *pTime,
                const char *pDebugDumpDir,
                map_crash_data_t& pCrashData)
{
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
    database->Connect();
    /* note: if [UUID,UID] record exists, pDebugDumpDir is not updated in the record */
    database->Insert_or_Update(pUUID, pUID, pDebugDumpDir, pTime);
    database_row_t row = database->GetRow(pUUID, pUID);
    database->DisConnect();

    mw_result_t res = FillCrashInfo(pUUID, pUID, pCrashData);
    if (res == MW_OK)
    {
        if (row.m_sReported == "1")
        {
            log("Crash is already reported");
            return MW_REPORTED;
        }
        if (row.m_sCount != "1")
        {
            log("Crash is in database already");
            return MW_OCCURED;
        }
    }
    return res;
}

std::string getDebugDumpDir(const char *pUUID, const char *pUID)
{
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
    database->Connect();
    database_row_t row = database->GetRow(pUUID, pUID);
    database->DisConnect();
    return row.m_sDebugDumpDir;
}

mw_result_t SaveDebugDump(const char *pDebugDumpDir,
                map_crash_data_t& pCrashData)
{
    std::string UID;
    std::string time;
    std::string analyzer;
    std::string executable;
    std::string cmdline;
    try
    {
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_TIME, time);
        dd.LoadText(FILENAME_UID, UID);
        dd.LoadText(FILENAME_ANALYZER, analyzer);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.LoadText(FILENAME_CMDLINE, cmdline);
    }
    catch (CABRTException& e)
    {
        error_msg("%s", e.what());
        if (e.type() == EXCEP_DD_SAVE)
        {
            return MW_FILE_ERROR;
        }
        return MW_ERROR;
    }

    if (IsDebugDumpSaved(UID.c_str(), pDebugDumpDir))
    {
        return MW_IN_DB;
    }

    mw_result_t res = SavePackageDescriptionToDebugDump(executable.c_str(), cmdline.c_str(), pDebugDumpDir);
    if (res != MW_OK)
    {
        return res;
    }

    std::string lUUID = GetLocalUUID(analyzer.c_str(), pDebugDumpDir);
    const char *uid_str = analyzer_has_InformAllUsers(analyzer.c_str())
        ? "-1"
        : UID.c_str();
    return SaveDebugDumpToDatabase(lUUID.c_str(), uid_str, time.c_str(), pDebugDumpDir, pCrashData);
}

mw_result_t FillCrashInfo(const char *pUUID,
                const char *pUID,
                map_crash_data_t& pCrashData)
{
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
    database->Connect();
    database_row_t row = database->GetRow(pUUID, pUID);
    database->DisConnect();

    std::string package;
    std::string executable;
    std::string description;
    std::string analyzer;
    try
    {
        CDebugDump dd;
        dd.Open(row.m_sDebugDumpDir.c_str());
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.LoadText(FILENAME_PACKAGE, package);
        dd.LoadText(FILENAME_DESCRIPTION, description);
        dd.LoadText(FILENAME_ANALYZER, analyzer);
    }
    catch (CABRTException& e)
    {
        error_msg("%s", e.what());
        return MW_ERROR;
    }

    pCrashData.clear();
    add_to_crash_data(pCrashData, CD_EXECUTABLE , executable.c_str()         );
    add_to_crash_data(pCrashData, CD_PACKAGE    , package.c_str()            );
    add_to_crash_data(pCrashData, CD_DESCRIPTION, description.c_str()        );
    add_to_crash_data(pCrashData, CD_UUID       , row.m_sUUID.c_str()        );
    add_to_crash_data(pCrashData, CD_UID        , row.m_sUID.c_str()         );
    add_to_crash_data(pCrashData, CD_COUNT      , row.m_sCount.c_str()       );
    add_to_crash_data(pCrashData, CD_TIME       , row.m_sTime.c_str()        );
    add_to_crash_data(pCrashData, CD_REPORTED   , row.m_sReported.c_str()    );
    add_to_crash_data(pCrashData, CD_MESSAGE    , row.m_sMessage.c_str()     );
    add_to_crash_data(pCrashData, CD_MWDDD      , row.m_sDebugDumpDir.c_str());
    add_to_crash_data(pCrashData, CD_MWANALYZER , analyzer.c_str()           );

    return MW_OK;
}

vector_pair_string_string_t GetUUIDsOfCrash(const char *pUID)
{
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
    vector_database_rows_t rows;
    database->Connect();
    rows = database->GetUIDData(pUID);
    database->DisConnect();

    vector_pair_string_string_t UUIDsUIDs;
    unsigned ii;
    for (ii = 0; ii < rows.size(); ii++)
    {
        UUIDsUIDs.push_back(make_pair(rows[ii].m_sUUID, rows[ii].m_sUID));
    }

    return UUIDsUIDs;
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
