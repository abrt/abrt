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


static void RunAnalyzerActions(const std::string& pAnalyzer, const std::string& pDebugDumpDir);


/**
 * Transforms a debugdump direcortry to inner crash
 * report form. This form is used for later reporting.
 * @param pDebugDumpDir A debugdump dir containing all necessary data.
 * @param pCrashReport A created crash report.
 */
static void DebugDumpToCrashReport(const std::string& pDebugDumpDir, map_crash_report_t& pCrashReport)
{
    std::string fileName;
    std::string content;
    bool isTextFile;
    CDebugDump dd;
    dd.Open(pDebugDumpDir);

    if (!dd.Exist(FILENAME_ARCHITECTURE) ||
        !dd.Exist(FILENAME_KERNEL) ||
        !dd.Exist(FILENAME_PACKAGE) ||
        !dd.Exist(FILENAME_COMPONENT) ||
        !dd.Exist(FILENAME_RELEASE) ||
        !dd.Exist(FILENAME_EXECUTABLE))
    {
        throw CABRTException(EXCEP_ERROR, "DebugDumpToCrashReport(): One or more of important file(s)'re missing.");
    }
    pCrashReport.clear();
    dd.InitGetNextFile();
    while (dd.GetNextFile(fileName, content, isTextFile))
    {
        if (!isTextFile)
        {
            add_crash_data_to_crash_report(pCrashReport,
                                           fileName,
                                           CD_BIN,
                                           CD_ISNOTEDITABLE,
                                           pDebugDumpDir + "/" + fileName);
        }
        else
        {
            if (fileName == FILENAME_ARCHITECTURE ||
                fileName == FILENAME_KERNEL ||
                fileName == FILENAME_PACKAGE ||
                fileName == FILENAME_COMPONENT ||
                fileName == FILENAME_RELEASE ||
                fileName == FILENAME_EXECUTABLE)
            {
                add_crash_data_to_crash_report(pCrashReport, fileName, CD_TXT, CD_ISNOTEDITABLE, content);
            }
            else if (fileName != FILENAME_UID &&
                     fileName != FILENAME_ANALYZER &&
                     fileName != FILENAME_TIME &&
                     fileName != FILENAME_DESCRIPTION )
            {
                if (content.length() < CD_ATT_SIZE)
                {
                    add_crash_data_to_crash_report(pCrashReport, fileName, CD_TXT, CD_ISEDITABLE, content);
                }
                else
                {
                    add_crash_data_to_crash_report(pCrashReport, fileName, CD_ATT, CD_ISEDITABLE, content);
                }
            }
        }
    }
}

/**
 * Get a local UUID from particular analyzer plugin.
 * @param pAnalyzer A name of an analyzer plugin.
 * @param pDebugDumpDir A debugdump dir containing all necessary data.
 * @return A local UUID.
 */
static std::string GetLocalUUID(const std::string& pAnalyzer,
                                      const std::string& pDebugDumpDir)
{
    CAnalyzer* analyzer = g_pPluginManager->GetAnalyzer(pAnalyzer);
    return analyzer->GetLocalUUID(pDebugDumpDir);
}

/**
 * Get a global UUID from particular analyzer plugin.
 * @param pAnalyzer A name of an analyzer plugin.
 * @param pDebugDumpDir A debugdump dir containing all necessary data.
 * @return A global UUID.
 */
static std::string GetGlobalUUID(const std::string& pAnalyzer,
                                       const std::string& pDebugDumpDir)
{
    CAnalyzer* analyzer = g_pPluginManager->GetAnalyzer(pAnalyzer);
    return analyzer->GetGlobalUUID(pDebugDumpDir);
}

/**
 * Take care of getting all additional data needed
 * for computing UUIDs and creating a report for particular analyzer
 * plugin. This report could be send somewhere afterwards.
 * @param pAnalyzer A name of an analyzer plugin.
 * @param pDebugDumpPath A debugdump dir containing all necessary data.
 */
static void CreateReport(const std::string& pAnalyzer,
                               const std::string& pDebugDumpDir)
{
    CAnalyzer* analyzer = g_pPluginManager->GetAnalyzer(pAnalyzer);
    analyzer->CreateReport(pDebugDumpDir);
}

mw_result_t CreateCrashReport(const std::string& pUUID,
                                                        const std::string& pUID,
                                                        map_crash_report_t& pCrashReport)
{
    VERB2 log("CreateCrashReport('%s','%s',result)", pUUID.c_str(), pUID.c_str());

    database_row_t row;
    if (pUUID != "")
    {
        CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
        database->Connect();
        row = database->GetUUIDData(pUUID, pUID);
        database->DisConnect();
    }
    if (pUUID == "" || row.m_sUUID != pUUID)
    {
        warn_client("CreateCrashReport(): UUID '"+pUUID+"' is not in database.");
        return MW_IN_DB_ERROR;
    }

    try
    {
        CDebugDump dd;
        std::string analyzer;
        std::string gUUID;

        VERB3 log(" LoadText(FILENAME_ANALYZER,'%s')", row.m_sDebugDumpDir.c_str());
        dd.Open(row.m_sDebugDumpDir);
        dd.LoadText(FILENAME_ANALYZER, analyzer);
        dd.Close();

        VERB3 log(" CreateReport('%s')", analyzer.c_str());
        CreateReport(analyzer, row.m_sDebugDumpDir);

        gUUID = GetGlobalUUID(analyzer, row.m_sDebugDumpDir);
        VERB3 log(" GetGlobalUUID:'%s'", gUUID.c_str());

        VERB3 log(" RunAnalyzerActions");
        RunAnalyzerActions(analyzer, row.m_sDebugDumpDir);
        VERB3 log(" DebugDumpToCrashReport");
        DebugDumpToCrashReport(row.m_sDebugDumpDir, pCrashReport);

        add_crash_data_to_crash_report(pCrashReport, CD_UUID, CD_TXT, CD_ISNOTEDITABLE, gUUID);
        add_crash_data_to_crash_report(pCrashReport, CD_MWANALYZER, CD_SYS, CD_ISNOTEDITABLE, analyzer);
        add_crash_data_to_crash_report(pCrashReport, CD_MWUID, CD_SYS, CD_ISNOTEDITABLE, pUID);
        add_crash_data_to_crash_report(pCrashReport, CD_MWUUID, CD_SYS, CD_ISNOTEDITABLE, pUUID);
        add_crash_data_to_crash_report(pCrashReport, CD_COMMENT, CD_TXT, CD_ISEDITABLE, "");
        add_crash_data_to_crash_report(pCrashReport, CD_REPRODUCE, CD_TXT, CD_ISEDITABLE, "1.\n2.\n3.\n");
    }
    catch (CABRTException& e)
    {
        warn_client("CreateCrashReport(): " + e.what());
        if (e.type() == EXCEP_DD_OPEN)
        {
            return MW_ERROR;
        }
        else if (e.type() == EXCEP_DD_LOAD)
        {
            return MW_FILE_ERROR;
        }
        else if (e.type() == EXCEP_PLUGIN)
        {
            return MW_PLUGIN_ERROR;
        }
        return MW_CORRUPTED;
    }

    return MW_OK;
}

void RunAction(const std::string& pActionDir,
                            const std::string& pPluginName,
                            const std::string& pPluginArgs)
{
    try
    {
        CAction* action = g_pPluginManager->GetAction(pPluginName);

        action->Run(pActionDir, pPluginArgs);
    }
    catch (CABRTException& e)
    {
        warn_client("RunAction(): " + e.what());
        update_client("Execution of '"+pPluginName+"' was not successful: " + e.what());
    }
}

void RunActionsAndReporters(const std::string& pDebugDumpDir)
{
    vector_pair_string_string_t::iterator it_ar = s_vectorActionsAndReporters.begin();
    for (; it_ar != s_vectorActionsAndReporters.end(); it_ar++)
    {
        try
        {
            if (g_pPluginManager->GetPluginType((*it_ar).first) == REPORTER)
            {
                CReporter* reporter = g_pPluginManager->GetReporter((*it_ar).first);

                map_crash_report_t crashReport;
                DebugDumpToCrashReport(pDebugDumpDir, crashReport);
                reporter->Report(crashReport, (*it_ar).second);
            }
            else if (g_pPluginManager->GetPluginType((*it_ar).first) == ACTION)
            {
                CAction* action = g_pPluginManager->GetAction((*it_ar).first);
                action->Run(pDebugDumpDir, (*it_ar).second);
            }
        }
        catch (CABRTException& e)
        {
            warn_client("RunActionsAndReporters(): " + e.what());
            update_client("Activation of plugin '"+(*it_ar).first+"' was not successful: " + e.what());
        }
    }
}


static bool CheckReport(const map_crash_report_t& pCrashReport)
{
    map_crash_report_t::const_iterator it_analyzer = pCrashReport.find(CD_MWANALYZER);
    map_crash_report_t::const_iterator it_mwuid = pCrashReport.find(CD_MWUID);
    map_crash_report_t::const_iterator it_mwuuid = pCrashReport.find(CD_MWUUID);

    map_crash_report_t::const_iterator it_package = pCrashReport.find(FILENAME_PACKAGE);
    map_crash_report_t::const_iterator it_architecture = pCrashReport.find(FILENAME_ARCHITECTURE);
    map_crash_report_t::const_iterator it_kernel = pCrashReport.find(FILENAME_KERNEL);
    map_crash_report_t::const_iterator it_component = pCrashReport.find(FILENAME_COMPONENT);
    map_crash_report_t::const_iterator it_release = pCrashReport.find(FILENAME_RELEASE);
    map_crash_report_t::const_iterator it_executable = pCrashReport.find(FILENAME_EXECUTABLE);

    if (it_analyzer == pCrashReport.end() || it_mwuid == pCrashReport.end() ||
        it_mwuuid == pCrashReport.end() || it_package == pCrashReport.end() ||
        it_architecture == pCrashReport.end() || it_kernel == pCrashReport.end() ||
        it_component == pCrashReport.end() || it_release == pCrashReport.end() ||
        it_executable == pCrashReport.end())
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

report_status_t Report(const map_crash_report_t& pCrashReport,
                                    const std::string& pUID)
{
    report_status_t ret;
    std::string key;
    std::string message;

    if (!CheckReport(pCrashReport))
    {
        throw CABRTException(EXCEP_ERROR, "Report(): Some of mandatory report data are missing.");
    }

    std::string analyzer = pCrashReport.find(CD_MWANALYZER)->second[CD_CONTENT];
    std::string UID = pCrashReport.find(CD_MWUID)->second[CD_CONTENT];
    std::string UUID = pCrashReport.find(CD_MWUUID)->second[CD_CONTENT];
    std::string packageNVR = pCrashReport.find(FILENAME_PACKAGE)->second[CD_CONTENT];
    std::string packageName = packageNVR.substr(0, packageNVR.rfind("-", packageNVR.rfind("-") - 1 ));

    // analyzer with package name (CCpp:xrog-x11-app) has higher priority
    key = analyzer + ":" + packageName;
    map_analyzer_actions_and_reporters_t::iterator keyPtr = s_mapAnalyzerActionsAndReporters.find(key);
    if (keyPtr == s_mapAnalyzerActionsAndReporters.end())
    {
        // if there is no such settings, then try default analyzer
        keyPtr = s_mapAnalyzerActionsAndReporters.find(analyzer);
    }

    if (keyPtr != s_mapAnalyzerActionsAndReporters.end())
    {
        vector_pair_string_string_t::iterator it_r = keyPtr->second.begin();
        for (; it_r != keyPtr->second.end(); it_r++)
        {
            std::string pluginName = it_r->first;
            try
            {
                std::string res;

                if (g_pPluginManager->GetPluginType(pluginName) == REPORTER)
                {
                    CReporter* reporter = g_pPluginManager->GetReporter(pluginName);
                    std::string home = "";
                    map_plugin_settings_t oldSettings;
                    map_plugin_settings_t newSettings;

                    if (pUID != "")
                    {
                        home = get_home_dir(atoi(pUID.c_str()));
                        if (home != "")
                        {
                            oldSettings = reporter->GetSettings();

                            if (LoadPluginSettings(home + "/.abrt/" + pluginName + "."PLUGINS_CONF_EXTENSION, newSettings))
                            {
                                reporter->SetSettings(newSettings);
                            }
                        }
                    }

                    res = reporter->Report(pCrashReport, it_r->second);

                    if (home != "")
                    {
                        reporter->SetSettings(oldSettings);
                    }
                }
                ret[pluginName].push_back("1");
                ret[pluginName].push_back(res);
                message += res + "\n";
            }
            catch (CABRTException& e)
            {
                ret[pluginName].push_back("0");
                ret[pluginName].push_back(e.what());
                warn_client("Report(): " + e.what());
                update_client("Reporting via '" + pluginName + "' was not successful: " + e.what());
            }
        }
    }

    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
    database->Connect();
    database->SetReported(UUID, UID, message);
    database->DisConnect();

    return ret;
}

void DeleteDebugDumpDir(const std::string& pDebugDumpDir)
{
    CDebugDump dd;
    dd.Open(pDebugDumpDir);
    dd.Delete();
}

std::string DeleteCrashInfo(const std::string& pUUID,
                                         const std::string& pUID)
{
    database_row_t row;
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
    database->Connect();
    row = database->GetUUIDData(pUUID, pUID);
    database->Delete(pUUID, pUID);
    database->DisConnect();

    return row.m_sDebugDumpDir;
}

/**
 * Check whether particular debugdump directory is saved
 * in database. This check is done together with an UID of an user.
 * @param pUID an UID of an user.
 * @param pDebugDumpDir A debugdump dir containing all necessary data.
 * @return It returns true if debugdump dir is already saved, otherwise
 * it returns false.
 */
static bool IsDebugDumpSaved(const std::string& pUID,
                                   const std::string& pDebugDumpDir)
{
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
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
static mw_result_t SavePackageDescriptionToDebugDump(const std::string& pExecutable,
                                                                        const std::string& pDebugDumpDir)
{
    std::string package;
    std::string packageName;

    if (pExecutable == "kernel")
    {
        packageName = package = "kernel";
    }
    else
    {
        package = GetPackage(pExecutable);
        packageName = package.substr(0, package.rfind("-", package.rfind("-") - 1));
        if (packageName == "" ||
            (g_setBlackList.find(packageName) != g_setBlackList.end()))
        {
            if (packageName == "")
            {
                error_msg("Executable doesn't belong to any package");
                return MW_PACKAGE_ERROR;
            }
            log("Blacklisted package");
            return MW_BLACKLISTED;
        }
        if (g_settings_bOpenGPGCheck)
        {
            if (!s_RPM.CheckFingerprint(packageName))
            {
                error_msg("package isn't signed with proper key");
                return MW_GPG_ERROR;
            }
            if (!CheckHash(packageName, pExecutable))
            {
                error_msg("executable has bad hash");
                return MW_GPG_ERROR;
            }
        }
    }

    std::string description = GetDescription(packageName);
    std::string component = GetComponent(pExecutable);

    try
    {
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.SaveText(FILENAME_PACKAGE, package);
        dd.SaveText(FILENAME_DESCRIPTION, description);
        dd.SaveText(FILENAME_COMPONENT, component);
    }
    catch (CABRTException& e)
    {
        warn_client("SavePackageDescriptionToDebugDump(): " + e.what());
        if (e.type() == EXCEP_DD_SAVE)
        {
            return MW_FILE_ERROR;
        }
        return MW_ERROR;
    }

    return MW_OK;
}

/**
 * Execute all action plugins, which are associated to
 * particular analyzer plugin.
 * @param pAnalyzer A name of an analyzer plugin.
 * @param pDebugDumpPath A debugdump dir containing all necessary data.
 */
static void RunAnalyzerActions(const std::string& pAnalyzer, const std::string& pDebugDumpDir)
{
    map_analyzer_actions_and_reporters_t::iterator analyzer = s_mapAnalyzerActionsAndReporters.find(pAnalyzer);
    if (analyzer != s_mapAnalyzerActionsAndReporters.end())
    {
        vector_pair_string_string_t::iterator it_a = analyzer->second.begin();
        for (; it_a != analyzer->second.end(); it_a++)
        {
            std::string pluginName = it_a->first;
            try
            {
                if (g_pPluginManager->GetPluginType(pluginName) == ACTION)
                {
                    CAction* action = g_pPluginManager->GetAction(pluginName);
                    action->Run(pDebugDumpDir, it_a->second);
                }
            }
            catch (CABRTException& e)
            {
                warn_client("RunAnalyzerActions(): " + e.what());
                update_client("Action performed by '" + pluginName + "' was not successful: " + e.what());
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
 * @param pCrashInfo A filled crash info.
 * @return It return results of operation. See mw_result_t.
 */
static mw_result_t SaveDebugDumpToDatabase(const std::string& pUUID,
                                                              const std::string& pUID,
                                                              const std::string& pTime,
                                                              const std::string& pDebugDumpDir,
                                                              map_crash_info_t& pCrashInfo)
{
    mw_result_t res;
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
    database_row_t row;
    database->Connect();
    database->Insert(pUUID, pUID, pDebugDumpDir, pTime);
    row = database->GetUUIDData(pUUID, pUID);
    database->DisConnect();
    res = GetCrashInfo(pUUID, pUID, pCrashInfo);
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
    return res;
}

mw_result_t SaveDebugDump(const std::string& pDebugDumpDir)
{
    map_crash_info_t info;
    return SaveDebugDump(pDebugDumpDir, info);
}

mw_result_t SaveDebugDump(const std::string& pDebugDumpDir,
                                                    map_crash_info_t& pCrashInfo)
{
    std::string lUUID;
    std::string UID;
    std::string time;
    std::string analyzer;
    std::string executable;
    mw_result_t res;

    try
    {
        CDebugDump dd;
        dd.Open(pDebugDumpDir);
        dd.LoadText(FILENAME_TIME, time);
        dd.LoadText(FILENAME_UID, UID);
        dd.LoadText(FILENAME_ANALYZER, analyzer);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
    }
    catch (CABRTException& e)
    {
        warn_client("SaveDebugDump(): " + e.what());
        if (e.type() == EXCEP_DD_SAVE)
        {
            return MW_FILE_ERROR;
        }
        return MW_ERROR;
    }

    if (IsDebugDumpSaved(UID, pDebugDumpDir))
    {
        return MW_IN_DB;
    }
    res = SavePackageDescriptionToDebugDump(executable, pDebugDumpDir);
    if (res != MW_OK)
    {
        return res;
    }

    lUUID = GetLocalUUID(analyzer, pDebugDumpDir);

    return SaveDebugDumpToDatabase(lUUID, UID, time, pDebugDumpDir, pCrashInfo);
}

mw_result_t GetCrashInfo(const std::string& pUUID,
                                                   const std::string& pUID,
                                                   map_crash_info_t& pCrashInfo)
{
    pCrashInfo.clear();
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
    database_row_t row;
    database->Connect();
    row = database->GetUUIDData(pUUID, pUID);
    database->DisConnect();

    std::string package;
    std::string executable;
    std::string description;

    try
    {
        CDebugDump dd;
        dd.Open(row.m_sDebugDumpDir);
        dd.LoadText(FILENAME_EXECUTABLE, executable);
        dd.LoadText(FILENAME_PACKAGE, package);
        dd.LoadText(FILENAME_DESCRIPTION, description);
    }
    catch (CABRTException& e)
    {
        warn_client("GetCrashInfo(): " + e.what());
        if (e.type() == EXCEP_DD_LOAD)
        {
            return MW_FILE_ERROR;
        }
        return MW_ERROR;
    }
    add_crash_data_to_crash_info(pCrashInfo, CD_EXECUTABLE, executable);
    add_crash_data_to_crash_info(pCrashInfo, CD_PACKAGE, package);
    add_crash_data_to_crash_info(pCrashInfo, CD_DESCRIPTION, description);
    add_crash_data_to_crash_info(pCrashInfo, CD_UUID, row.m_sUUID);
    add_crash_data_to_crash_info(pCrashInfo, CD_UID, row.m_sUID);
    add_crash_data_to_crash_info(pCrashInfo, CD_COUNT, row.m_sCount);
    add_crash_data_to_crash_info(pCrashInfo, CD_TIME, row.m_sTime);
    add_crash_data_to_crash_info(pCrashInfo, CD_REPORTED, row.m_sReported);
    add_crash_data_to_crash_info(pCrashInfo, CD_MESSAGE, row.m_sMessage);
    add_crash_data_to_crash_info(pCrashInfo, CD_MWDDD, row.m_sDebugDumpDir);

    return MW_OK;
}

vector_pair_string_string_t GetUUIDsOfCrash(const std::string& pUID)
{
    CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase);
    vector_database_rows_t rows;
    database->Connect();
    rows = database->GetUIDData(pUID);
    database->DisConnect();

    vector_pair_string_string_t UUIDsUIDs;
    int ii;
    for (ii = 0; ii < rows.size(); ii++)
    {
        UUIDsUIDs.push_back(make_pair(rows[ii].m_sUUID, rows[ii].m_sUID));
    }

    return UUIDsUIDs;
}

void AddAnalyzerActionOrReporter(const std::string& pAnalyzer,
                                              const std::string& pAnalyzerOrReporter,
                                              const std::string& pArgs)
{
    s_mapAnalyzerActionsAndReporters[pAnalyzer].push_back(make_pair(pAnalyzerOrReporter, pArgs));
}

void AddActionOrReporter(const std::string& pActionOrReporter,
                                      const std::string& pArgs)
{
    s_vectorActionsAndReporters.push_back(make_pair(pActionOrReporter, pArgs));
}
